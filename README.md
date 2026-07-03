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

```

## Requirements

| Requirement | version        |
| ----------- | -------------- |
| c++ version | >C++17         |
| compiler    | clang++ or g++ |

You can use other compilers, but clang and g are the recommended compilers for this library.

> 💡 **Tip:** \*If you are using the recommended compilers, you can use the following argument to tell the compiler to use C++17: `-std=c++17`\*
