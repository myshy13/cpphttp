
#include "test.hpp"
#include "server.hpp"
#include <arpa/inet.h>
#include <atomic>
#include <chrono>
#include <cstring>
#include <iostream>
#include <netdb.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>

int main() {
  const int port = 8000;
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

  app.get("/echo/:key/:value", [](Request &req, Response &res) {
    std::stringstream ss;

    ss << "KEY: " << req.params["key"] << "\n";
    ss << "VALUE: " << req.params["value"] << "\n";

    res.status(200).send(ss.str());
  });

  app.post("/echo/:key/:value", [](Request &req, Response &res) {
    std::stringstream ss;
    ss << "KEY: " << req.params["key"] << "\n";
    ss << "VALUE: " << req.params["value"] << "\n";

    res.status(200).send(ss.str());
  });

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
