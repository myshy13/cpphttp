#pragma once

#include "colors.hpp"
#include "server.hpp"
#include <string>
class TestPlugin : public ServerPlugin {
public:
  TestPlugin(bool &allPassed) : allPassed_(allPassed) {}

  void onAfterRequest(Request &req, Response &) override {
    if (req.path == "/plugin") {
      std::string apple;
      if (req.context.get("apple", apple) && apple == "banana") { // middleware sets it to banana
        std::cout << Colors::BRIGHT_GREEN << "[PASS] " << Colors::NC
                  << "Plugin context\n";
      } else {
        std::cout << Colors::BRIGHT_RED << "[FAIL] " << Colors::NC
                  << "Plugin context\n";
        allPassed_ = false;
      }
    }
  }

private:
  bool& allPassed_;
};