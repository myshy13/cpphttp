#include "server.hpp"

#include "tls.hpp"
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

inline std::mutex logMutex;
Server::~Server() = default;

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

void Server::handleConnection(const std::string &raw, int client_fd,
                              SSL *ssl /* = nullptr */) {
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

  std::string response = "HTTP/1.1 " + std::to_string(res.statusCode) + " " +
                         getStatusPhrase(res.statusCode) + "\r\n" +
                         "Content-Length: " + std::to_string(res.body.size()) +
                         "\r\n" +
                         // concatenate all accumulated headers …
                         [&] {
                           std::string h;
                           for (const auto &[k, v] : res.headers)
                             h += k + ": " + v + "\r\n";
                           return h;
                         }() +
                         "Connection: close\r\n\r\n" + res.body;

  if (ssl) {
    // TLS path – use OpenSSL's write routine.
    SSL_write(ssl, response.c_str(), static_cast<int>(response.size()));
  } else {
    // Plain socket path – same as before.
    ::send(client_fd, response.c_str(), static_cast<ssize_t>(response.size()),
           0);
  }
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

// ===================== Https logic =====================
void Server::secure(const std::string &privateKeyFile,
                    const std::string &publicCertFile) {
  tls_ = std::make_unique<TLSContext>(publicCertFile, privateKeyFile);
}

struct IOWrapper {
  int fd;
  SSL *ssl; // nullptr for plain sockets
  std::array<char, BUFFER_SIZE> buf{};

  // read up to BUFFER_SIZE bytes, returns -1 on error / EOF.
  ssize_t read() const {
    if (ssl) {
      int r = SSL_read(ssl, const_cast<char *>(buf.data()), buf.size());
      if (r <= 0) {
        int err = SSL_get_error(ssl, r);
        if (err == SSL_ERROR_WANT_READ)
          return 0;
        return -1;
      }
      return r;
    }
    return ::recv(fd, const_cast<char *>(buf.data()), buf.size(), 0);
  }

  // write a whole buffer, returns true on success.
  bool write(const std::string &data) const {
    if (ssl) {
      const char *p = data.c_str();
      size_t left = data.size();
      while (left) {
        int w = SSL_write(ssl, p, static_cast<int>(left));
        if (w <= 0) {
          int err = SSL_get_error(ssl, w);
          if (err == SSL_ERROR_WANT_WRITE)
            continue;
          return false;
        }
        p += w;
        left -= w;
      }
      return true;
    }
    return ::send(fd, data.c_str(), data.size(), 0) == (ssize_t)data.size();
  }

  // clean shutdown for TLS connections
  void shutdown() const {
    if (ssl) {
      SSL_shutdown(ssl);
      SSL_free(ssl);
    }
    ::close(fd);
  }
};

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

//====== IO struct ======

namespace {
  struct IO {
    int fd;
    SSL *ssl;

    std::array<char, BUFFER_SIZE> buf{};

    ssize_t read() const {
      if (ssl) {
        int r = SSL_read(ssl, const_cast<char *>(buf.data()),
                         static_cast<int>(buf.size()));
        if (r <= 0) {
          int err = SSL_get_error(ssl, r);
          if (err == SSL_ERROR_WANT_READ)
            return 0;
          return -1;
        }
        return r;
      }
      return ::recv(fd, const_cast<char *>(buf.data()),
                    static_cast<ssize_t>(buf.size()), 0);
    }

    bool write(const std::string &data) const {
      if (ssl) {
        const char *p = data.c_str();
        size_t remaining = data.size();
        while (remaining) {
          int w = SSL_write(ssl, p, static_cast<int>(remaining));
          if (w <= 0) {
            int err = SSL_get_error(ssl, w);
            if (err == SSL_ERROR_WANT_WRITE)
              continue;
            return false;
          }
          p += w;
          remaining -= w;
        }
        return true;
      }
      return ::send(fd, data.c_str(), static_cast<ssize_t>(data.size()), 0) ==
             (ssize_t)data.size();
    }

    // shutdown
    void close() const {
      if (ssl) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
      }
      ::close(fd);
    }
  };
} // namespace

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
      /* --------------------------------------------------------------
                Minimal I/O wrapper that can talk either plain sockets
                or TLS (OpenSSL) sockets.
                --------------------------------------------------------------
   */
      struct IO {
        int fd;
        SSL *ssl = nullptr; // nullptr → plain
        std::array<char, BUFFER_SIZE> buf{};
        ssize_t read() const {
          if (ssl) {
            int r = SSL_read(ssl, const_cast<char *>(buf.data()),
                             static_cast<int>(buf.size()));
            if (r <= 0) {
              int err = SSL_get_error(ssl, r);
              if (err == SSL_ERROR_WANT_READ)
                return 0;
              return -1;
            }
            return r;
          }
          return ::recv(fd, const_cast<char *>(buf.data()),
                        static_cast<ssize_t>(buf.size()), 0);
        }
        bool write(const std::string &data) const {
          if (ssl) {
            const char *p = data.c_str();
            size_t left = data.size();
            while (left) {
              int w = SSL_write(ssl, p, static_cast<int>(left));
              if (w <= 0) {
                int err = SSL_get_error(ssl, w);
                if (err == SSL_ERROR_WANT_WRITE)
                  continue;
                return false;
              }
              p += w;
              left -= w;
            }
            return true;
          }
          return ::send(fd, data.c_str(), static_cast<ssize_t>(data.size()),
                        0) == static_cast<ssize_t>(data.size());
        }
        void close() const {
          if (ssl) {
            SSL_shutdown(ssl);
            SSL_free(ssl);
          }
          ::close(fd);
        }
      };
      IO io{client_fd, nullptr};
      // ------- TLS handshake (if TLS was enabled) -----------------
      if (tls_) {
        io.ssl = tls_->makeSSL(client_fd);
        if (!TLSContext::doHandshake(io.ssl)) {
          io.close(); // handshake failed → drop
          return;
        }
      }
      // ------- Read the HTTP request --------------------------------
      std::string request;
      bool headersRead = false;
      size_t expectedContentLength = 0;
      while (true) {
        ssize_t n = io.read();
        if (n <= 0)
          break; // error / client closed
        request.append(io.buf.data(), static_cast<size_t>(n));

        if (!headersRead) {
          auto headerEnd = request.find("\r\n\r\n");
          if (headerEnd != std::string::npos) {
            headersRead = true;
            Request tempReq = parseRequest(request);
            expectedContentLength = tempReq.contentLength;
          }
        }

        if (headersRead) {
          auto headerEnd = request.find("\r\n\r\n");
          if (headerEnd != std::string::npos &&
              request.size() >= headerEnd + 4 + expectedContentLength) {
            break;
          }
        }
      }
      // ------- Dispatch ------------------------------------------------
      if (!request.empty())
        handleConnection(request, client_fd, io.ssl);
      // ------- Clean shutdown ---------------------------------------
      io.close();
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