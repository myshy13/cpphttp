# cpphttp

![GitHub commit activity](https://img.shields.io/github/commit-activity/y/myshy13/cpphttp)
![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=c%2B%2B&logoColor=white)
![Express Inspired](https://img.shields.io/badge/inspired%20by-ExpressJS-black?logo=express)

A simple and easy-to-use HTTP server inspired by Express.js

## Installation

To install the library, go to [the main .hpp file](https://github.com/myshy13/cpphttp/blob/main/server.hpp) and click download. Then, include it in your project.

## Usage

Here is a boilerplate/ example file to get started.

```cpp
#include "server.hpp"
#include <iostream>

int main() {
  const int port = 8080;
  Server app = server::createServer(port);
  if (app.port == -1) {
    return 1;
  }

  app.get("/", [](Request &, Response &res) {
    res.status(200).send("This is my c++ server!");
  });

  app.listen([=]()->void {
    std::cout << "listening on: http://localhost:";
    std::cout << port << std::endl;
  });
}
```

## Advanced Routing with Router

For applications requiring complex resource mapping or middleware
  stacking, introduce a `Router` instance. The `cpphttp::Server` now supports mounting this custom router using
  `server::use(router)`. This pattern decouples high-level routing logic from the core server loop, leading to cleaner, modular
  services.

### Example: Defining and Using a Router

```cpp
#include "server.hpp"
#include <iostream>

int main() {
  const int port = 8080;
  Server app =
  server::createServer(port);
  if (app.port == -1) {
    return 1;
  }

  const router = server::createRouter();

  router.get("/", [](Request &, Response &res) {
    res.status(200).send("This is from a router");
  });

  app.listen([=]()->void {
    std::cout << "listening on: http://localhost:";
    std::cout << port << std::endl;
  });
}
```

## Requirements

| Requirement | version        |
| ----------- | -------------- |
| c++ version | >C++17         |

If you are on windows, use the Visual Studio Compiler to build and run your code.

> 💡 **Tip:** *
