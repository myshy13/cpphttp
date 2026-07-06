#ifndef MYSHY13_RATELIMIT_PLUGIN
#define MYSHY13_RATELIMIT_PLUGIN
#include "server.hpp"
#include <algorithm> // Needed for std::remove_if
#include <iostream>
// NOTE: The use of the `std::stringstream` in `split()` requires including
// <sstream>.

// ============================== Rate Limiting State Management
// =====================
namespace global_rate_limiter {

  struct IpUsage {
    std::vector<TimePoint> timestamps;
  };

  // Central storage: IP Address -> Usage History
  inline std::unordered_map<std::string, IpUsage> usageStore;
  inline std::mutex storeMutex;

  /**
   * @brief Records a request and prunes old entries.
   */
  inline void recordRequest(const std::string &ip) {
    // Lock the state while we are modifying it (critical section)
    std::lock_guard<std::mutex> lock(storeMutex);

    auto now = std::chrono::steady_clock::now();

    if (usageStore.find(ip) == usageStore.end()) {
      // Initialize if first time seeing this IP
      usageStore[ip].timestamps.clear();
    }

    usageStore[ip].timestamps.push_back(now);
  }

  /**
   * @brief Counts how many requests within the last 'window' duration.
   */
  inline int countRequestsInWindow(const std::string &ip,
                            const std::chrono::seconds &window) {
    std::lock_guard<std::mutex> lock(storeMutex);
    auto it = usageStore.find(ip);

    if (it == usageStore.end()) {
      return 0; // IP not tracked yet
    }

    auto &usage = it->second;
    auto now = std::chrono::steady_clock::now();
    int recentCount = 0;

    // Iterate and count valid timestamps
    for (const auto &timestamp : usage.timestamps) {
      if (std::chrono::duration_cast<std::chrono::seconds>(now - timestamp)
              .count() < window.count()) {
        recentCount++;
      }
    }
    return recentCount;
  }

  /**
   * @brief Removes extremely old requests to save memory.
   */
  inline void pruneOldRequests(const std::string &ip,
                        const std::chrono::seconds &window) {
    std::lock_guard<std::mutex> lock(storeMutex);
    auto it = usageStore.find(ip);
    if (it == usageStore.end())
      return;

    auto now = std::chrono::steady_clock::now();
    // Remove any timestamp that is older than the window defined by 'window'
    auto &timestamps = it->second.timestamps;
    timestamps.erase(
        std::remove_if(
            timestamps.begin(), timestamps.end(),
            [&](const TimePoint &t) {
              return std::chrono::duration_cast<std::chrono::seconds>(now - t)
                         .count() >= window.count();
            }),
        timestamps.end());

    // Optional: If the list becomes empty, remove the IP from the map to save
    // memory.
  }
} // namespace global_rate_limiter

/**
 * @brief The Rate Limit Plugin Implementation.
 */
class RateLimitPlugin : public ServerPlugin {
private:
  int maxRequests;
  std::chrono::seconds timeWindow;

public:
  // Constructor allows configuration of the throttle rules (e.g., 5 requests
  // per minute)
  RateLimitPlugin(int limit = 5, int window_seconds = 60)
      : maxRequests(limit), timeWindow(window_seconds) {
    std::cout << "RateLimitPlugin initialized: Limit=" << maxRequests
              << " / Window=" << timeWindow.count() << "s\n";
  }

  void onInit(Server & /*server*/) override {
    // Nothing required here.
  }

  /**
   * @brief The core throttling logic runs before the request handler.
   * This function must enforce the limits or allow continuation.
   */
  void onBeforeRequest(Request &req, Response &res,
                       TimePoint /*startTime*/) override {
    const std::string clientIp = req.ip;

    // 1. Check current usage against the limit
    int recentCount =
        global_rate_limiter::countRequestsInWindow(clientIp, timeWindow);

    if (recentCount >= maxRequests) {
      // --- POLICY VIOLATION: BLOCK REQUEST ---

      // Set the status code to 429 Too Many Requests
      res.status(429);

      // Inform the client how long they need to wait (e.g., the window
      // duration)
      std::string retryAfter = std::to_string(timeWindow.count());
      res.setHeader("Retry-After", retryAfter);

      // Stop processing by sending a predefined response body and exiting
      res.send("Rate limit exceeded. Please slow down.");
      // Execution stops here because the Response object was modified to send
      // data.

    } else {
      // --- POLICY OK: ALLOW REQUEST ---

      // Record the successful request before allowing it to proceed
      global_rate_limiter::recordRequest(clientIp);
      // Note: We call prune periodically (or rely on a background cleanup
      // thread). For simplicity, we only rely on the check function's implicit
      // counting.
    }
  }
};

#endif