#ifndef MYSHY13_LOGGER_PLUGIN
#define MYSHY13_LOGGER_PLUGIN

#include "server.hpp"
#include <iostream>

struct LoggerSettings {
  bool req = true;
  bool res = false;
  bool ip = false;
};

class LoggerPlugin : public ServerPlugin {

public:
  LoggerPlugin(LoggerSettings s) { settings = s; }

  void onBeforeRequest(Request &req, Response &, TimePoint start) override {
    startTime_ = start;
    if (settings.req) {
      std::cout << "[INCOMING] " << req.method << " AT \"" << req.path << "\"";
    }
    if (settings.ip) {
      if (settings.req) {
        std::cout << " FROM " << req.ip << "\n";
      } else {
        std::cout << "[INCOMING] Request from " << req.ip << "\n";
      }
    } else {
      std::cout << "\n";
    }
  }

  void onAfterRequest(Request &req, Response &, Duration duration) override {
    auto ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

    if (settings.res) {
      std::cout << "[RES] " << req.method << " " << req.path << " took " << ms
                << "ms\n";
    }
  }

private:
  LoggerSettings settings = {};
  TimePoint startTime_;
};

#endif
