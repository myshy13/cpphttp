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

// 1. Define a
  custom router implementation (implementation details omitted for brevity)
class MyRouter : public Router {
public:
    void
  setupRoutes(Server& server) override {
        // Example: setting up a specific path handler on the router instance itself

      server.get("/api/v1/users", [](Request &req, Response &res) {
            res.json({"status": "ok", "message":
  "Fetching all users via custom Router!"});
        });
    }
};

int main() {
  const int port = 8080;
  Server app =
  server::createServer(port);

  // 2. Create and register the router with the server
  MyRouter userApiRouter;

  app.use(userApiRouter); // <--- New feature in action!

  if (app.port == -1) {
    return 1;
  }

  app.listen([=]()->void
  {
    std::cout << "listening on: http://localhost:";
    std::cout << port << std::endl;
  });
}
```
*(Note: The
  `Router` class and its implementation details would reside in their own files, which are assumed to be available to the
  compiler.)*

## Requirements

| Requirement | version        |
| ----------- | -------------- |
| c++ version | >C++17         |
| Compiler    | clang++ or g++ |

You can use other compilers, but clang++ or g++ are recommended compilers for this
  library.
If you are on windows, use Visual studio to compile your code.

> 💡 **Tip:** *
