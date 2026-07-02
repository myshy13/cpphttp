#include "server.hpp"
#include <iostream>

int main() {
  const int port = 8080;
  Server app = server::createServer(port);
  if (app.port == -1) {
    std::cerr << "Error binding to port\n";
    return 1;
  }

  app.get("/", [](Request &req, Response &res) {
    res.setHeader("Content-Type", "text/plain");
    res.status(200).sendFile("log.txt");
  });

  app.use("/", Method::ALL, [](Request &req, Response &res, NextHandler next) {
    std::cout << "USER IP: " << req.ip << "\n";
    next(req, res);
  });

  app.listen([]() {
    std::cout << "Listening on http://localhost:";
    std::cout << port << "\n";
  });
}