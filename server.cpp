#include "server.hpp"

std::ofstream logFile("log.txt");

namespace server {
std::vector<int> activePorts;
}

std::optional<Method> stringToMethod(const std::string& s) {
  static const std::unordered_map<std::string, Method> table = {
      {"GET", Method::GET},     {"POST", Method::POST},
      {"PATCH", Method::PATCH}, {"DELETE", Method::DELETE},
      {"HEAD", Method::HEAD},   {"OPTIONS", Method::OPTIONS},
  };

  auto it = table.find(s);
  if (it == table.end()) return std::nullopt;

  return it->second;
}

std::string methodToString(Method method) {
  switch (method) {
    case Method::GET:
      return "GET";
    case Method::POST:
      return "POST";
    case Method::PATCH:
      return "PATCH";
    case Method::DELETE:
      return "DELETE";
    case Method::HEAD:
      return "HEAD";
    case Method::OPTIONS:
      return "OPTIONS";
    case Method::PUT:
      return "PUT";
    case Method::ALL:
      return "GET, POST, PATCH, DELETE, HEAD, OPTIONS, PUT";
  }

  return {};
}

std::string contentTypeFromExtension(const std::string& filename) {
  static const std::unordered_map<std::string, std::string> mimeTypes = {
      {".html", "text/html"},        {".htm", "text/html"},
      {".css", "text/css"},          {".js", "application/javascript"},
      {".json", "application/json"}, {".png", "image/png"},
      {".jpg", "image/jpeg"},        {".jpeg", "image/jpeg"},
      {".gif", "image/gif"},         {".svg", "image/svg+xml"},
      {".txt", "text/plain"},        {".ico", "image/x-icon"},
      {".pdf", "application/pdf"},   {".xml", "application/xml"},
      {".zip", "application/zip"},   {".mp3", "audio/mpeg"},
      {".mp4", "video/mp4"},
  };

  std::string ext = fs::path(filename).extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

  auto it = mimeTypes.find(ext);
  if (it != mimeTypes.end()) return it->second;

  return {};
}

Response& Response::status(int code) {
  statusCode = code;
  return *this;
}

Response& Response::setHeader(std::string key, std::string value) {
  headers[key] = value;
  return *this;
}

Response& Response::send(const std::string& text) {
  body = text;
  return *this;
}

Response& Response::sendFile(const std::string& filename) {
  std::ifstream file(filename);

  if (file.is_open()) {
    std::stringstream buffer;
    buffer << file.rdbuf();
    body = buffer.str();
  } else {
    std::cerr << "Failed to open the file.\n";
  }

  return *this;
}

Request parseRequest(const std::string& raw) {
  Request req;

  std::istringstream stream(raw);
  std::string line;

  std::getline(stream, line);

  std::istringstream lineStream(line);
  lineStream >> req.method >> req.path >> req.version;

  while (std::getline(stream, line) && line != "\r" && !line.empty()) {
    auto colon = line.find(':');

    if (colon == std::string::npos) continue;

    std::string key = line.substr(0, colon);
    std::string value = line.substr(colon + 2);

    if (!value.empty() && value.back() == '\r') value.pop_back();

    req.headers[key] = value;
  }

  return req;
}

void Server::get(std::string path, Handler handler) {
  routes.push_back({Method::GET, path, handler});
}

void Server::post(std::string path, Handler handler) {
  routes.push_back({Method::POST, path, handler});
}

void Server::patch(std::string path, Handler handler) {
  routes.push_back({Method::PATCH, path, handler});
}

void Server::delete_(std::string path, Handler handler) {
  routes.push_back({Method::DELETE, path, handler});
}

void Server::put(std::string path, Handler handler) {
  routes.push_back({Method::PUT, path, handler});
}

void Server::head(std::string path, Handler handler) {
  routes.push_back({Method::HEAD, path, handler});
}

void Server::options(std::string path, Handler handler) {
  routes.push_back({Method::OPTIONS, path, handler});
}

void Server::staticDir(std::string prefix, std::string path) {
  staticDirs.push_back({prefix, path});
}

void Server::cors(std::string prefix = "/", std::string allowedOrigins = "*", Method allowedMethods = Method::ALL) {
  corsOptions.push_back({allowedOrigins, allowedMethods});
}

void Server::listen(std::function<void()> callback) {
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);

  if (server_fd < 0) {
    perror("socket");
    return;
  }

  int opt = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(port);

  if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
    perror("bind");
    return;
  }

  if (::listen(server_fd, 16) < 0) {
    perror("listen");
    return;
  }

  callback();

  while (true) {
    int client_fd = accept(server_fd, nullptr, nullptr);

    if (client_fd < 0) {
      perror("accept");
      continue;
    }

    char buf[BUFFER_SIZE];

    ssize_t n = recv(client_fd, buf, sizeof(buf) - 1, 0);

    if (n > 0) {
      buf[n] = '\0';
      handleConnection(buf, client_fd);
    }

    close(client_fd);
  }

  close(server_fd);
}

void Server::use(std::string prefix, Method allowedMethods,
                 std::function<void(Request&, Response&, NextHandler)> handle) {
  Middleware middleware;
  middleware.prefix = prefix;
  middleware.allowedMethods = allowedMethods;
  middleware.handle = handle;

  this->middlewares.push_back(middleware);
}

void Server::handleConnection(const std::string& raw, int client_fd) {
  Request req = parseRequest(raw);
  Response res;

  std::string clientIp = "unknown";
  sockaddr_in peerAddr{};
  socklen_t peerLen = sizeof(peerAddr);

  if (getpeername(client_fd, reinterpret_cast<sockaddr*>(&peerAddr),
                  &peerLen) == 0) {
    char ipBuffer[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &peerAddr.sin_addr, ipBuffer, sizeof(ipBuffer))) {
      clientIp = ipBuffer;
    }
  }

  if (auto forwardedIt = req.headers.find("X-Forwarded-For");
      forwardedIt != req.headers.end() && !forwardedIt->second.empty()) {
    clientIp = forwardedIt->second;
    if (auto commaPos = clientIp.find(','); commaPos != std::string::npos) {
      clientIp = clientIp.substr(0, commaPos);
    }
  }

  req.ip = clientIp;

  bool matched = false;
  bool handlerInvoked = false;

  auto dispatchRequest = [&](Request& req, Response& res) {
    handlerInvoked = true;

    for (auto& route : routes) {
      if (route.path == req.path &&
          route.method == stringToMethod(req.method)) {
        route.handler(req, res);
        matched = true;
        return;
      }
    }

    for (auto& dir : staticDirs) {
      if (req.path.find(dir.prefix) == 0) {
        try {
          if (fs::exists(dir.path) && fs::is_directory(dir.path)) {
            for (const auto& entry : fs::directory_iterator(dir.path)) {
              std::string entryname = fs::path(entry.path().filename().string())
                                          .filename()
                                          .string();
              std::string requestfile = fs::path(req.path).filename().string();

              if (entryname == requestfile) {
                std::string filepath = entry.path().string();

                if (fs::is_regular_file(filepath)) {
                  res.headers["Content-Type"] =
                      contentTypeFromExtension(filepath);
                  res.sendFile(filepath);
                  matched = true;
                } else {
                  res.status(500).send("Internal Server Error");
                }

                break;
              }
            }
          } else {
            std::cerr << "Path does not exist or is not a directory.\n";
          }
        } catch (const fs::filesystem_error& e) {
          std::cerr << "Error: " << e.what() << "\n";
        }

        break;
      }
    }
  };

  std::vector<std::reference_wrapper<const Middleware>> activeMiddlewares;

  for (const auto& middleware : middlewares) {
    if (req.path.find(middleware.prefix) == 0 &&
        (middleware.allowedMethods == Method::ALL ||
         middleware.allowedMethods == stringToMethod(req.method))) {
      activeMiddlewares.push_back(std::cref(middleware));
    }
  }

  std::function<void(size_t, Request&, Response&)> runMiddlewares;
  runMiddlewares = [&](size_t index, Request& req, Response& res) {
    if (index >= activeMiddlewares.size()) {
      dispatchRequest(req, res);
      return;
    }

    const Middleware& middleware = activeMiddlewares[index].get();
    middleware.handle(req, res, [&](Request& req, Response& res) {
      runMiddlewares(index + 1, req, res);
    });
  };

  if (activeMiddlewares.empty()) {
    dispatchRequest(req, res);
  } else {
    runMiddlewares(0, req, res);
  }

  if (logsEnabled) {
    logFile << "New request: " << req.method << " " << req.path;
  }

  if (!matched) {
    res.status(404).send("Not Found");
  }

  std::string headers;

  for (const auto& [key, value] : res.headers) {
    headers += key;
    headers += ": ";
    headers += value;
    headers += "\r\n";
  }

  for (auto& cors : corsOptions) {
    if (req.path.find(cors.prefix) == 0) {
      res.headers["Access-Control-Allow-Origin"] = cors.allowedOrigins;
      res.headers["Access-Control-Allow-Methods"] = methodToString(cors.allowedMethods);
    }
  }

  for (auto& route : routes) {
      if (route.path == req.path &&
          route.method == stringToMethod(req.method)) {
        route.handler(req, res);
        matched = true;
        return;
      }
    }

  std::string response = "HTTP/1.1 " + std::to_string(res.statusCode) +
                         " OK\r\n"
                         "Content-Length: " +
                         std::to_string(res.body.size()) + "\r\n" + headers +
                         "Connection: close\r\n\r\n" + res.body;

  send(client_fd, response.c_str(), response.size(), 0);

  if (logsEnabled) {
    logFile << " Response: ";
    logFile << res.statusCode << "\n";
    logFile.flush();
  }
}

namespace server {

Server createServer(int port) {
  if (port > 65535) {
    std::cerr << "Error: port too high\n";
    return {-1, {}};
  }

  if (port <= 0) {
    std::cerr << "Error: port too low\n";
    return {-1, {}};
  }

  return {port, {}};
}

}  // namespace server