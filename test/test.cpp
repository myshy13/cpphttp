#include "server.hpp"
#include "plugins/logger.hpp"
#include "plugins/timer.hpp"
#include <arpa/inet.h>
#include <atomic>
#include <chrono>
#include <cstring>
#include <iostream>
#include <netdb.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <vector>

static std::string sendRawRequest(int port, const std::string &request) {
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  addrinfo *result = nullptr;
  int rc = getaddrinfo("127.0.0.1", std::to_string(port).c_str(), &hints, &result);
  if (rc != 0 || result == nullptr) {
    return "getaddrinfo failed";
  }

  int sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
  if (sock < 0) {
    freeaddrinfo(result);
    return "socket failed";
  }

  if (connect(sock, result->ai_addr, result->ai_addrlen) != 0) {
    close(sock);
    freeaddrinfo(result);
    return "connect failed";
  }

  freeaddrinfo(result);

  ssize_t sent = send(sock, request.data(), request.size(), 0);
  if (sent < 0) {
    close(sock);
    return "send failed";
  }

  std::string response;
  char buffer[4096];
  while (true) {
    ssize_t received = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (received <= 0) {
      break;
    }
    buffer[received] = '\0';
    response += buffer;
  }

  close(sock);
  return response;
}

static std::string makeRequest(int port,
                               const std::string &method,
                               const std::string &target,
                               const std::string &body = "",
                               const std::vector<std::pair<std::string, std::string>> &headers = {}) {
  std::string request = method + " " + target + " HTTP/1.1\r\n";
  request += "Host: localhost\r\n";

  for (auto &header : headers) {
    request += header.first + ": " + header.second + "\r\n";
  }

  if (!body.empty()) {
    request += "Content-Length: " + std::to_string(body.size()) + "\r\n";
  }

  request += "Connection: close\r\n\r\n";
  request += body;
  return sendRawRequest(port, request);
}

static bool checkResponse(const std::string &response,
                          const std::string &expected,
                          const char *label) {
  bool passed = response.find(expected) != std::string::npos;
  std::cout << "[" << (passed ? "PASS" : "FAIL") << "] " << label << std::endl;
  if (!passed) {
    std::cout << "  Expected substring: " << expected << std::endl;
    std::cout << "  Actual response: " << response << std::endl;
  }
  return passed;
}

int main() {
  const int port = 8080;
  Server app = server::createServer(port);
  if (app.port == -1) {
    return 1;
  }

  app.cors("/", "*", Method::ALL);

  app.get("/", [](Request &, Response &res) {
    res.status(200).send("This is my c++ server!");
  });

  app.get("/hello", [](Request &, Response &res) {
    res.status(200).send("hello from server");
  });

  app.get("/status", [](Request &, Response &res) {
    res.status(201).send("created");
  });

  app.post("/echo", [](Request &, Response &res) {
    res.status(200).send("echo");
  });

  app.registerPlugin(std::make_unique<LoggerPlugin>());
  app.registerPlugin(std::make_unique<TimerPlugin>());

  std::atomic<bool> serverReady(false);
  std::thread serverThread([&]() {
    app.listen([&]() {
      std::cout << "listening on: http://localhost:" << port << std::endl;
      serverReady = true;
    });
  });

  for (int i = 0; i < 20 && !serverReady.load(); ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  if (!serverReady.load()) {
    std::cerr << "Server failed to start." << std::endl;
    if (serverThread.joinable()) {
      serverThread.detach();
    }
    return 1;
  }

  bool allPassed = true;

  std::string root = makeRequest(port, "GET", "/");
  allPassed &= checkResponse(root, "HTTP/1.1 200", "GET / status code");
  allPassed &= checkResponse(root, "This is my c++ server!", "GET / response body");

  std::string hello = makeRequest(port, "GET", "/hello");
  allPassed &= checkResponse(hello, "HTTP/1.1 200", "GET /hello status code");
  allPassed &= checkResponse(hello, "hello from server", "GET /hello response body");

  std::string status = makeRequest(port, "GET", "/status");
  allPassed &= checkResponse(status, "HTTP/1.1 201", "GET /status status code");
  allPassed &= checkResponse(status, "created", "GET /status response body");

  std::string echo = makeRequest(port, "POST", "/echo", "ping", {{"Content-Type", "text/plain"}});
  allPassed &= checkResponse(echo, "HTTP/1.1 200", "POST /echo status code");
  allPassed &= checkResponse(echo, "echo", "POST /echo response body");

  std::string options = makeRequest(port, "OPTIONS", "/");
  allPassed &= checkResponse(options, "HTTP/1.1", "OPTIONS / response");
  allPassed &= checkResponse(options, "Access-Control-Allow-Origin", "CORS header");

  if (allPassed) {
    std::cout << "All tests passed." << std::endl;
  } else {
    std::cout << "Some tests failed." << std::endl;
  }

  if (serverThread.joinable()) {
    serverThread.detach();
  }

  return allPassed ? 0 : 1;
}
