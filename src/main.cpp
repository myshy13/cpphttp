#include "server.hpp"
#include <iostream>

int main() {
  const int port = 8080;
  Server app = server::createServer(port);
  if (app.port == -1) {
    return 1;
  }

  app.staticDir("static", "./");

  app.listen([=]() -> void {
    std::cout << "listening on: https://localhost:";
    std::cout << port << std::endl;
  });
}
