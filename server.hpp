#ifndef MYSHY13_HTTPSERVER
#define MYSHY13_HTTPSERVER

#include <algorithm>
#include <arpa/inet.h>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <netinet/in.h>
#include <optional>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

extern std::ofstream logFile;

constexpr int BUFFER_SIZE = 30720;

enum class Method { GET, POST, PATCH, DELETE, HEAD, OPTIONS, ALL };

std::optional<Method> stringToMethod(const std::string &s);

struct Path {
  Method Type;
  std::string path;
};

struct Request {
  std::string method;
  std::string path;
  std::string version;
  std::unordered_map<std::string, std::string> headers;
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

struct Server {
  int port;
  std::vector<Route> routes;
  std::vector<Directory> staticDirs;

  void get(std::string path, Handler handler);
  void post(std::string path, Handler handler);
  void staticDir(std::string prefix, std::string path);
  void listen();

private:
  void handleConnection(const std::string &raw, int client_fd);
};

namespace server {

  extern std::vector<int> activePorts;

  Server createServer(int port);

}

#endif