#ifndef MYSHY13_FILE_LOGGER_PLUGIN
#define MYSHY13_FILE_LOGGER_PLUGIN

#include "server.hpp"
#include <fstream>
#include <mutex>

class FileLoggerPlugin : public ServerPlugin {
public:
  explicit FileLoggerPlugin(std::string path = "log.txt")
      : path_(std::move(path)) {}

  void onBeforeRequest(Request &req, Response &) override {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ofstream log(path_, std::ios::app);
    if (log.is_open()) {
      log << "[before] " << req.method << ' ' << req.path << " from " << req.ip
          << '\n';
    }
  }

  void onAfterRequest(Request &req, Response &res) override {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ofstream log(path_, std::ios::app);
    if (log.is_open()) {
      log << "[after] " << req.method << ' ' << req.path << " -> "
          << res.statusCode << '\n';
    }
  }

private:
  std::string path_;
  std::mutex mutex_;
};

#endif
