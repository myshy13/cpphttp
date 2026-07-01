# cpphttp

A simple and easy-to-use HTTP server inspired by Express.js

## Installation

To install the library, go to [the main .hpp file](https://github.com/myshy13/cpphttp/blob/main/server.hpp) and click download. Then, include it in your project.

## Usage

Here is a boilerplate/ example file to get started.

```cpp
#include "server.hpp"
#include <iostream>

int main() {
  Server app = server::createServer(8080); // Create a server on port 8080
  if (app.port == -1) {
    std::cerr << "Error binding to port\n"; // error if the port is already in use or if the port is invalid
    return 1;
  }

  app.get("/", [](Request &req, Response &res) {
    res.setHeader("Content-Type", "text/plain"); // Set the response to plaintext
    res.status(200).sendFile("Hello world"); // send the text "Hello world" to the client
  });

  app.listen(); // bind to the port and start listening for requests
}
```

## Requirements

| Name         |                |
| ------------ | -------------- |
| c++ version  |                |
| c++ compiler | clang++ or g++ |
