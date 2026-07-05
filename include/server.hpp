#ifndef MYSHY13_HTTPSERVER
#define MYSHY13_HTTPSERVER

#include "nlohmann/json.hpp"
#include "threadpool.hpp"

#include <arpa/inet.h>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <netinet/in.h>
#include <optional>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <unordered_map>
#include <variant>
#include <vector>

struct Server;

// ===================== global runtime state =====================

inline std::mutex serverRegistryMutex;
inline std::vector<Server*> activeServers;

inline std::atomic<bool> shutdownRequested{false};

// ===================== time types =====================

using TimePoint = std::chrono::steady_clock::time_point;
using Duration  = std::chrono::nanoseconds;

// ===================== HTTP types =====================

enum class Method {
  GET, POST, PATCH, DELETE, HEAD, OPTIONS, PUT, ALL
};

enum class ContentType {
  Text, Json, UrlEncoded, Binary
};

constexpr int BUFFER_SIZE = 30720;


// ===================== helpers =====================

inline std::optional<Method> stringToMethod(std::string_view s) {
  static const std::unordered_map<std::string_view, Method> table = {
      {"GET", Method::GET},
      {"POST", Method::POST},
      {"PATCH", Method::PATCH},
      {"DELETE", Method::DELETE},
      {"HEAD", Method::HEAD},
      {"OPTIONS", Method::OPTIONS},
      {"PUT", Method::PUT},
  };

  if (auto it = table.find(s); it != table.end())
    return it->second;

  return std::nullopt;
}

inline std::string methodToString(Method method) {
  switch (method) {
    case Method::GET: return "GET";
    case Method::POST: return "POST";
    case Method::PATCH: return "PATCH";
    case Method::DELETE: return "DELETE";
    case Method::HEAD: return "HEAD";
    case Method::OPTIONS: return "OPTIONS";
    case Method::PUT: return "PUT";
    case Method::ALL: return "GET, POST, PATCH, DELETE, HEAD, OPTIONS, PUT";
  }
  return {};
}

inline std::string getStatusPhrase(int code) {
  switch (code) {
    case 200: return "OK";
    case 404: return "Not Found";
    case 500: return "Internal Server Error";
    default: return "OK";
  }
}

// ===================== request/response =====================

using FormData = std::unordered_map<std::string, std::string>;

struct Request {
  std::string method;
  std::string path;
  std::string version;

  std::unordered_map<std::string, std::string> headers;

  std::string ip;
  ContentType contentType;

  std::variant<
    std::string,
    std::vector<uint8_t>,
    nlohmann::json,
    FormData,
    const char*
  > body;

  int contentLength = 0;
  TimePoint startTime;
};

struct Response {
  int statusCode = 200;
  std::string body;

  std::unordered_map<std::string, std::string> headers = {
    {"Content-Type", "text/plain"}
  };

  Response& status(int code);
  Response& setHeader(std::string key, std::string value);
  Response& send(const std::string text);
  Response& sendFile(const std::string filename);
};

// ===================== routing =====================

using Handler = std::function<void(Request&, Response&)>;

struct Route {
  Method method;
  std::string path;
  Handler handler;
};

struct Directory {
  std::string prefix;
  std::string path;
};

using NextHandler = std::function<void(Request&, Response&)>;

struct Middleware {
  std::string prefix = "/";
  bool enabled = true;
  Method allowedMethods = Method::ALL;

  std::function<void(Request&, Response&, NextHandler)> handle;
};

struct Cors {
  std::string allowedOrigins = "*";
  Method allowedMethods = Method::ALL;
  std::string prefix = "/";
};

// ===================== server plugin =====================

class ServerPlugin {
public:
  std::string prefix = "/";
  Method allowedMethods = Method::ALL;

  virtual ~ServerPlugin() = default;

  virtual void onInit(Server&) {}
  virtual void onBeforeRequest(Request&, Response&, TimePoint) {}
  virtual void onAfterRequest(Request&, Response&, Duration) {}
  virtual void onShutdown(Server&) {}
};

// ===================== server =====================

struct Server {
  Server();
  explicit Server(int port);

  std::vector<std::unique_ptr<ServerPlugin>> plugins;

  int port = 0;

  std::vector<Route> routes;
  std::vector<Directory> staticDirs;
  std::vector<Middleware> middlewares;
  std::vector<Cors> corsOptions;

  int server_fd_ = -1;
  std::atomic<bool> running_{true};

  void get(std::string path, Handler handler);
  void post(std::string path, Handler handler);
  void patch(std::string path, Handler handler);
  void delete_(std::string path, Handler handler);
  void put(std::string path, Handler handler);
  void head(std::string path, Handler handler);
  void options(std::string path, Handler handler);

  void staticDir(std::string prefix, std::string path);

  void listen(std::function<void()> callback);
  void use(const std::string prefix, Method allowedMethods,
           std::function<void(Request&, Response&, NextHandler)> handle);

  void cors(const std::string prefix = "/",
            const std::string allowedOrigins = "*",
            Method allowedMethods = Method::ALL);

  void requestStop();

  void registerPlugin(std::unique_ptr<ServerPlugin> plugin) {
    plugin->onInit(*this);
    plugins.push_back(std::move(plugin));
  }

private:
  ThreadPool pool{4};
  void handleConnection(const std::string raw, int client_fd);
};

// ===================== signal =====================

inline void handleSignal(int) {
  shutdownRequested.store(true, std::memory_order_relaxed);

  std::lock_guard<std::mutex> lock(serverRegistryMutex);

  for (Server* s : activeServers) {
    if (s) {
      s->requestStop();
    }
  }
}

namespace server {
  Server createServer(int port);
}

#endif