#ifndef MYSHY13_REQUEST_DURATION_PLUGIN
#define MYSHY13_REQUEST_DURATION_PLUGIN

#include "server.hpp"

#include <chrono>
#include <iostream>
#include <mutex>
#include <unordered_map>

class RequestDurationPlugin : public ServerPlugin {
public:
  void onBeforeRequest(Request &req, Response &) override {
    auto now = std::chrono::steady_clock::now();

    std::lock_guard<std::mutex> lock(mutex_);
    startTimes_[&req] = now;
  }

  void onAfterRequest(Request &req, Response &) override {
    auto end = std::chrono::steady_clock::now();

    std::lock_guard<std::mutex> lock(mutex_);

    auto it = startTimes_.find(&req);
    if (it == startTimes_.end()) {
      return; // safety: should not happen
    }

    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - it->second);

    startTimes_.erase(it);

    std::cout << "[request] " << req.method << " " << req.path << " took "
              << duration.count() / 1000.0 << " ms\n";
  }

private:
  std::mutex mutex_;
  std::unordered_map<Request *, std::chrono::steady_clock::time_point>
      startTimes_;
};

#endif