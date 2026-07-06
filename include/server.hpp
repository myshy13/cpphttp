#ifndef MYSHY13_HTTPSERVER
#define MYSHY13_HTTPSERVER

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
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

inline std::vector<std::string> split(const std::string &s, char delimiter) {
  std::vector<std::string> tokens;
  std::stringstream ss(s);
  std::string token;

  while (std::getline(ss, token, delimiter)) {
    tokens.push_back(token);
  }

  return tokens;
}

/**
 * @brief Compares the parameterized route template against the actual request
 * path to extract parameter names and assign them values.
 *
 * @param route The path template (e.g., /api/v1/:key/:value)
 * @param request The concrete URL received (e.g., /api/v1/that/this)
 * @return std::unordered_map<std::string, std::string> A map of parameter names
 * to values.
 */
inline std::unordered_map<std::string, std::string>
getParams(const std::string &route, const std::string &request) {
  std::unordered_map<std::string, std::string> param_map;

  // 1. Tokenize both paths
  std::vector<std::string> route_segments = split(route, '/');
  std::vector<std::string> request_segments = split(request, '/');

  // The loop should run up to the length of the shorter path array
  // to prevent out-of-bounds errors.
  size_t limit = std::min(route_segments.size(), request_segments.size());

  // 2. Iterate and compare segments
  for (size_t i = 0; i < limit; ++i) {
    const std::string &route_segment = route_segments[i];
    const std::string &request_segment = request_segments[i];

    // Check if the segment in the ROUTE starts with a parameter marker (':')
    if (!route_segment.empty() && route_segment[0] == ':') {

      // This is a parameterized slot (e.g., ":key")

      // Extract the param name by removing the leading ':'
      std::string param_name = route_segment.substr(1);

      // The actual value must be the corresponding request segment.
      // We only map it if the request segment is not empty (e.g., handling
      // trailing slashes)
      if (!request_segment.empty()) {
        // Store in the map: Key = param name, Value = request segment content
        param_map[param_name] = request_segment;
      }
    }
  }

  return param_map;
}

/**
 * @brief Determines if the request path structurally matches the route
 * template.
 *
 * This function performs a segment-by-segment comparison, allowing for
 * parameters (segments starting with ':'). It also ensures all segments match
 * in length.
 *
 * NOTE: The params are extracted and stored in req BEFORE this check is needed
 * in your main loop, but we re-run the logic here to make it self-contained.
 */
inline bool isStructuralMatch(const std::string &route_path,
                              const std::string &request_path) {
  std::vector<std::string> route_segments = split(route_path, '/');
  std::vector<std::string> request_segments = split(request_path, '/');

  // 1. Check length equality (A strict router requires paths to have the same
  // number of segments)
  if (route_segments.size() != request_segments.size()) {
    return false;
  }

  // 2. Segment-by-Segment Comparison
  for (size_t i = 0; i < route_segments.size(); ++i) {
    const std::string &r_segment = route_segments[i];
    const std::string &q_segment = request_segments[i];

    // Skip empty segments if your split function creates them (this is common
    // in URL parsing)
    if (r_segment.empty() && q_segment.empty()) {
      continue;
    }

    // A) Check for Dynamic Match (The Fix!)
    // Does the route segment start with ':', indicating a parameter slot?
    if (!r_segment.empty() && r_segment[0] == ':') {
      // This always matches structurally, because that's the point of
      // parameters.
      continue;
    }

    // B) Check for Exact Match (Static segment)
    if (r_segment != q_segment) {
      // If it is a static segment and the strings do not match, the paths do
      // NOT align.
      return false;
    }
  }

  // If we get here, every segment either matched exactly OR was correctly
  // handled as a dynamic parameter.
  return true;
}

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
  std::unordered_map<std::string, std::string> params;

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