#include "plugins/logger.hpp"
#include "plugins/timer.hpp"

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

  app.registerPlugin(std::make_unique<FileLoggerPlugin>("log.txt"));
  app.registerPlugin(std::make_unique<RequestDurationPlugin>());

  app.listen([]() {
    std::cout << "listening on: http://localhost:";
    std::cout << port << std::endl;
  });
}
