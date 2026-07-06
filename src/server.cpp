#include "server.hpp"

#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

inline std::mutex logMutex;

namespace fs = std::filesystem;

// predifinitions (only if needed)
Request parseRequest(const std::string raw);

// ===================== mime types =====================

std::string contentTypeFromExtension(const std::string filename) {
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
  if (it != mimeTypes.end())
    return it->second;

  return "application/octet-stream";
}

// ===================== response =====================

Response &Response::status(int code) {
  statusCode = code;
  return *this;
}

Response &Response::setHeader(std::string key, std::string value) {
  headers[key] = value;
  return *this;
}

Response &Response::send(const std::string text) {
  body = text;
  return *this;
}

Response &Response::sendFile(const std::string filename) {
  std::ifstream file(filename);

  if (!file.is_open()) {
    statusCode = 404;
    body = "Not Found";
    return *this;
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  body = buffer.str();

  return *this;
}

// ===================== request parsing =====================

void Server::handleConnection(const std::string raw, int client_fd) {
  Request req = parseRequest(raw);
  Response res;

  std::string clientIp = "unknown";
  sockaddr_in peerAddr{};
  socklen_t peerLen = sizeof(peerAddr);

  if (getpeername(client_fd, reinterpret_cast<sockaddr *>(&peerAddr),
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

  // ---- body parsing ----
  if (req.contentLength > 0) {
    auto contentTypeIt = req.headers.find("Content-Type");

    if (contentTypeIt != req.headers.end()) {
      std::string contentTypeValue = contentTypeIt->second;

      if (contentTypeValue.find("application/json") != std::string::npos) {
        req.contentType = ContentType::Json;
      } else if (contentTypeValue.find("application/x-www-form-urlencoded") !=
                 std::string::npos) {
        req.contentType = ContentType::UrlEncoded;
      } else if (contentTypeValue.find("text/plain") != std::string::npos) {
        req.contentType = ContentType::Text;
      } else {
        req.contentType = ContentType::Binary;
      }
    } else {
      req.contentType = ContentType::Binary;
    }

    size_t headerEnd = raw.find("\r\n\r\n");

    if (headerEnd != std::string::npos) {
      std::string bodyStr = raw.substr(headerEnd + 4);

      if (req.contentType == ContentType::Json) {
        try {
          nlohmann::json jsonBody = nlohmann::json::parse(bodyStr);
          req.body = jsonBody;
        } catch (const nlohmann::json::exception &err) {
          req.body = std::string("Invalid JSON");

          std::lock_guard<std::mutex> lock(logMutex);
          std::cerr << "WARNING: " << err.what() << std::endl;
        }
      } else if (req.contentType == ContentType::UrlEncoded) {
        FormData formData;
        std::istringstream formStream(bodyStr);
        std::string pair;

        while (std::getline(formStream, pair, '&')) {
          size_t eqPos = pair.find('=');
          if (eqPos != std::string::npos) {
            std::string key = pair.substr(0, eqPos);
            std::string value = pair.substr(eqPos + 1);
            formData[key] = value;
          }
        }

        req.body = formData;
      } else if (req.contentType == ContentType::Text) {
        req.body = bodyStr;
      } else {
        std::vector<uint8_t> binaryBody(bodyStr.begin(), bodyStr.end());
        req.body = binaryBody;
      }
    }
  }

  bool matched = false;

  auto dispatchRequest = [&](Request &req, Response &res) {
    std::chrono::steady_clock::time_point startTime;

    for (auto &plugin : plugins) {
      if (req.path.find(plugin->prefix) == 0 &&
          (plugin->allowedMethods == Method::ALL ||
           plugin->allowedMethods == stringToMethod(req.method))) {
        plugin->onBeforeRequest(req, res);
      }
    }

    for (auto &route : routes) {
      if (isStructuralMatch(route.path, req.path) &&
          route.method == stringToMethod(req.method)) {
        req.params = getParams(route.path, req.path);
        route.handler(req, res);
        matched = true;
        return;
      }
    }

    for (auto &dir : staticDirs) {
      if (req.path.find(dir.prefix) == 0) {
        try {
          if (fs::exists(dir.path) && fs::is_directory(dir.path)) {
            std::string relPath = req.path.substr(dir.prefix.length());
            fs::path safePath = fs::path(dir.path) / relPath;

            if (fs::exists(safePath) && fs::is_regular_file(safePath)) {
              res.headers["Content-Type"] =
                  contentTypeFromExtension(safePath.string());
              res.sendFile(safePath.string());
              matched = true;
            } else {
              res.status(404).send("Not Found");
            }
          }
        } catch (const fs::filesystem_error &err) {
          std::lock_guard<std::mutex> lock(logMutex);
          std::cerr << "ERROR: FS filesystem error: " << err.what() << "\n";
        }

        break;
      }
    }
  };

  std::vector<std::reference_wrapper<const Middleware>> activeMiddlewares;

  for (const auto &middleware : middlewares) {
    if (req.path.find(middleware.prefix) == 0 &&
        (middleware.allowedMethods == Method::ALL ||
         middleware.allowedMethods == stringToMethod(req.method))) {
      activeMiddlewares.push_back(std::cref(middleware));
    }
  }

  std::function<void(size_t, Request &, Response &)> runMiddlewares;

  runMiddlewares = [&](size_t index, Request &req, Response &res) {
    if (index >= activeMiddlewares.size()) {
      dispatchRequest(req, res);
      return;
    }

    const Middleware &middleware = activeMiddlewares[index].get();

    middleware.handle(req, res, [&](Request &req, Response &res) {
      runMiddlewares(index + 1, req, res);
    });
  };

  if (activeMiddlewares.empty()) {
    dispatchRequest(req, res);
  } else {
    runMiddlewares(0, req, res);
  }

  for (auto &plugin : plugins) {
    if (req.path.find(plugin->prefix) == 0 &&
        (plugin->allowedMethods == Method::ALL ||
         plugin->allowedMethods == stringToMethod(req.method))) {
      plugin->onAfterRequest(req, res);
    }
  }

  if (!matched) {
    res.status(404).send("Not Found");
  }

  for (auto &cors : corsOptions) {
    if (req.path.find(cors.prefix) == 0) {
      res.headers["Access-Control-Allow-Origin"] = cors.allowedOrigins;
      res.headers["Access-Control-Allow-Methods"] =
          methodToString(cors.allowedMethods);
    }
  }

  std::string headers;

  for (const auto &[key, value] : res.headers) {
    headers += key + ": " + value + "\r\n";
  }

  std::string response =
      "HTTP/1.1 " + std::to_string(res.statusCode) + " " +
      getStatusPhrase(res.statusCode) +
      "\r\nContent-Length: " + std::to_string(res.body.size()) + "\r\n" +
      headers + "Connection: close\r\n\r\n" + res.body;

  send(client_fd, response.c_str(), response.size(), 0);
}

Request parseRequest(const std::string raw) {
  Request req;

  std::istringstream stream(raw);
  std::string line;

  std::getline(stream, line);

  std::istringstream lineStream(line);
  lineStream >> req.method >> req.path >> req.version;

  while (std::getline(stream, line) && !line.empty() && line != "\r") {
    auto colon = line.find(':');
    if (colon == std::string::npos)
      continue;

    std::string key = line.substr(0, colon);
    std::string value = line.substr(colon + 2);

    if (!value.empty() && value.back() == '\r')
      value.pop_back();

    req.headers[key] = value;

    if (key == "Content-Length") {
      try {
        req.contentLength = std::stoi(value);
      } catch (...) {
        req.contentLength = 0;
      }
    }
  }

  return req;
}

// ===================== server methods =====================
Server::Server(int port) : port(port) {
  std::lock_guard<std::mutex> lock(serverRegistryMutex);
  activeServers.push_back(this);
}

// ==================== Server and router ====================

Server server::createServer(int port) { return Server(port); }

Router server::createRouter() { return Router(); }

// ==================== route helpers ====================
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

void Server::cors(const std::string prefix, const std::string allowedOrigins,
                  Method allowedMethods) {
  corsOptions.push_back({allowedOrigins, allowedMethods, prefix});
}

// ===================== Router routing :) ===================
void Router::get(std::string path, Handler handler) {
  routes.push_back({Method::GET, path, handler});
}

void Router::post(std::string path, Handler handler) {
  routes.push_back({Method::POST, path, handler});
}

void Router::patch(std::string path, Handler handler) {
  routes.push_back({Method::PATCH, path, handler});
}

void Router::delete_(std::string path, Handler handler) {
  routes.push_back({Method::DELETE, path, handler});
}

void Router::put(std::string path, Handler handler) {
  routes.push_back({Method::PUT, path, handler});
}

void Router::head(std::string path, Handler handler) {
  routes.push_back({Method::HEAD, path, handler});
}

void Router::options(std::string path, Handler handler) {
  routes.push_back({Method::OPTIONS, path, handler});
}

void Router::staticDir(std::string prefix, std::string path) {
  staticDirs.push_back({prefix, path});
}

// ===================== stop logic =====================
void Server::requestStop() {
  running_ = false;

  if (server_fd_ != -1) {
    shutdown(server_fd_, SHUT_RDWR);

    // wake accept()
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, (sockaddr *)&addr, sizeof(addr)) < 0) {
      // ignore errors; this is only to wake accept()
    }
    close(sock);
  }
}

// ===================== listen loop =====================

void Server::listen(std::function<void()> callback) {
  std::signal(SIGINT, handleSignal);

  server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd_ < 0)
    return;

  int opt = 1;
  setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(port);

  if (bind(server_fd_, (sockaddr *)&addr, sizeof(addr)) < 0)
    return;

  if (::listen(server_fd_, 16) < 0)
    return;

  callback();

  while (running_.load() && !shutdownRequested.load()) {
    int client_fd = accept(server_fd_, nullptr, nullptr);

    if (client_fd < 0) {
      if (errno == EINTR)
        continue;
      break;
    }

    pool.enqueue([this, client_fd]() {
      std::array<char, BUFFER_SIZE> buf{};
      std::string request;

      while (true) {
        ssize_t n = recv(client_fd, buf.data(), buf.size(), 0);

        if (n <= 0)
          break;

        request.append(buf.data(), static_cast<size_t>(n));

        if (request.find("\r\n\r\n") != std::string::npos)
          break;
      }

      if (!request.empty())
        handleConnection(request, client_fd);

      close(client_fd);
    });
  }

  running_ = false;

  if (server_fd_ != -1) {
    shutdown(server_fd_, SHUT_RDWR);
    close(server_fd_);
  }

  server_fd_ = -1;

  pool.stop();

  for (auto &plugin : plugins) {
    plugin->onShutdown(*this);
  }
}

// ===================== middleware =====================

void Server::use(
    const std::string prefix, Method allowedMethods,
    std::function<void(Request &, Response &, NextHandler)> handle) {

  Middleware m;
  m.prefix = prefix;
  m.allowedMethods = allowedMethods;
  m.handle = handle;

  middlewares.push_back(m);
}

void Server::use(const Router &router) {
  for (const Route &route : router.routes) {
    routes.push_back(route);
  }

  for (const Directory &staticDir : router.staticDirs) {
    staticDirs.push_back(staticDir);
  }
}