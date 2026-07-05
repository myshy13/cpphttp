#ifndef MYSHY13_FILE_LOGGER_PLUGIN
#define MYSHY13_FILE_LOGGER_PLUGIN

#include "server.hpp"
#include <fstream>
#include <iostream>
#include <mutex>

class LoggerPlugin : public ServerPlugin {
public:
  void onBeforeRequest(Request &req, Response &, TimePoint start) override {
    startTime_ = start;
    std::cout << "[REQ] " << req.method << " " << req.path << "\n";
  }

  void onAfterRequest(Request &req, Response &, Duration duration) override {
    auto ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

    std::cout << "[RES] " << req.method << " " << req.path << " took " << ms
              << "ms\n";
  }

private:
  TimePoint startTime_;
};

#endif
