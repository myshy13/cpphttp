#ifndef TIMER_PLUGIN_HPP
#define TIMER_PLUGIN_HPP

#include "server.hpp"
#include <chrono>
#include <iostream>

class TimerPlugin : public ServerPlugin {
public:
  void onBeforeRequest(Request &, Response &, TimePoint start) override {
    start_ = start;
  }

  void onAfterRequest(Request &, Response &, Duration duration) override {
    auto ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    std::cout << "[TIMER] request took " << ms << "ms\n";
  }

private:
  TimePoint start_;
};

#endif