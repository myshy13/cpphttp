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

## Requirements

| Requirement | version        |
| ----------- | -------------- |
| c++ version | >C++17         |
| Compiler    | clang++ or g++ |

You can use other compilers, but clang++ or g++ are recommended compilers for this library.
If you are on windows, use Visual studio to compile your code.

> 💡 **Tip:** \*If you are using one of the recommended compilers, you can use the following argument to tell the compiler to use C++17: `-std=c++17` to make it permanent, run this command depending on your compiler:
>
> ```shell
> function g++() {
>     command /usr/bin/g++ -std=c++17 "$@"
> }
> ```
>
> ```shell
> function clang++() {
>     command /usr/bin/clang++ -std=c++17 "$@"
> }
> ```

## Changelog
