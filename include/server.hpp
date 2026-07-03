#ifndef MYSHY13_HTTPSERVER
#define MYSHY13_HTTPSERVER

#include "nlohmann/json.hpp"
#include "threadpool.hpp"
#include <algorithm>
#include <arpa/inet.h>
#include <condition_variable>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <mutex>
#include <netinet/in.h>
#include <optional>
#include <queue>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <vector>

inline std::string getStatusPhrase(int code) {
  switch (code) {
  case 200:
    return "OK";
  case 404:
    return "Not Found";
  case 500:
    return "Internal Server Error";
  default:
    return "OK";
  }
}

namespace fs = std::filesystem;

extern std::ofstream logFile;

constexpr int BUFFER_SIZE = 30720;

enum class Method { GET, POST, PATCH, DELETE, HEAD, OPTIONS, PUT, ALL };

std::optional<Method> stringToMethod(const std::string &s);

struct Path {
  Method Type;
  std::string path;
};

enum class ContentType { Text, Json, UrlEncoded, Binary };

using FormData = std::unordered_map<std::string, std::string>;

struct Request {
  std::string method;
  std::string path;
  std::string version;
  std::unordered_map<std::string, std::string> headers;
  std::string ip;
  ContentType contentType;
  std::variant<std::string, std::vector<uint8_t>, nlohmann::json, FormData,
               const char *>
      body;
  int contentLength = 0;
};

std::string contentTypeFromExtension(const std::string &filename);

struct Response {
  int statusCode = 200;
  std::string body;
  std::unordered_map<std::string, std::string> headers = {
      {"Content-Type", "text/plain"},
  };

  Response &status(int code);
  Response &setHeader(std::string key, std::string value);
  Response &send(const std::string &text);
  Response &sendFile(const std::string &filename);
};

using Handler = std::function<void(Request &, Response &)>;

struct Route {
  Method method;
  std::string path;
  Handler handler;
};

Request parseRequest(const std::string &raw);

struct Directory {
  std::string prefix;
  std::string path;
};

using NextHandler = std::function<void(Request &, Response &)>;

struct Middleware {
  std::string prefix = "/";
  bool enabled = true;
  Method allowedMethods = Method::ALL;
  std::function<void(Request &, Response &, NextHandler)> handle;
};

struct Cors {
  std::string allowedOrigins = "*";
  Method allowedMethods = Method::ALL;
  std::string prefix = "/";
};

struct Server {
  int port;
  bool logsEnabled = false;
  std::vector<Route> routes;
  std::vector<Directory> staticDirs;
  std::vector<Middleware> middlewares;
  std::vector<Cors> corsOptions;

  Server(int port = 0, bool logsEnabled = false)
      : port(port), logsEnabled(logsEnabled) {}

  void get(std::string path, Handler handler);
  void post(std::string path, Handler handler);
  void patch(std::string path, Handler handler);
  void delete_(std::string path, Handler handler);
  void put(std::string path, Handler handler);
  void head(std::string path, Handler handler);
  void options(std::string path, Handler handler);
  void staticDir(std::string prefix, std::string path);
  void listen(std::function<void()> callback);
  void use(std::string prefix, Method allowedMethods,
           std::function<void(Request &, Response &, NextHandler)> handle);
  void cors(std::string prefix = "/", std::string allowedOrigins = "*",
            Method allowedMethods = Method::ALL);

private:
  void handleConnection(const std::string &raw, int client_fd);
  ThreadPool pool{4};
};

namespace server {

  extern std::vector<int> activePorts;

  Server createServer(int port, bool logsEnabled = false);

}

#endif
