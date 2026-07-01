#include "server.hpp"
#include <iostream>

int main() {
  Server app = server::createServer(8080);
  if (app.port == -1) {
    std::cerr << "Error binding to port\n";
    return 1;
  }

  app.get("/logs", [](Request &req, Response &res) {
    res.setHeader("Content-Type", "text/plain");
    res.status(200).sendFile("log.txt");
  });

  app.use("/", Method::ALL, [](Request &req, Response &res, NextHandler next) {
    std::cout << "Middleware: " << req.method << " " << req.path << "\n";
    next(req, res);
  });

  app.listen();
}