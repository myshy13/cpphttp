#include "server.hpp"
#include <iostream>

using json = nlohmann::json;

void middleware(Request &req, Response &res, NextHandler next) {
  std::cout << "Middleware: " << req.method << " " << req.path << "\n";
  next(req, res);
}

int main() {
  const int port = 8080;
  Server app = server::createServer(port, false);
  if (app.port == -1) {
    std::cerr << "Error binding to port\n";
    return 1;
  }

  app.get("/", [](Request &req, Response &res) {
    res.status(200).send("This is my c++ server!");
  });

  app.use("/", Method::ALL, middleware);

  app.listen([]() {
    std::cout << "Listening on http://localhost:";
    std::cout << port << "\n";
  });
}
