# cpphttp

![GitHub commit activity](https://img.shields.io/github/commit-activity/y/myshy13/cpphttp)
![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=c%2B%2B&logoColor=white)
![Express Inspired](https://img.shields.io/badge/inspired%20by-ExpressJS-black?logo=express)

## Overview

`cpphttp` is a lightweight C++ HTTP server library inspired by **Express.js**. It provides a simple, expressive API for defining routes, middleware, static file serving, CORS handling and a plugin system. The library is intended for learning, prototyping and experimentation **and is NOT recommended for production use**.

## Features

- Route handling for all standard HTTP methods (`GET`, `POST`, `PUT`, `DELETE`, `PATCH`, `HEAD`, `OPTIONS`, `ALL`).
- Parameterised routes (`/users/:id`).
- `Router` objects for modular route grouping.
- Middleware pipeline with prefix and method filtering.
- Built‑in CORS support.
- Static file serving with MIME‑type detection.
- Plugin system (`ServerPlugin`) for custom request/response hooks.
- Thread‑pooled connection handling for concurrent requests.
- Dependency on **nlohmann/json** for JSON body parsing.
- Requires only a C++17 compiler and POSIX sockets.

## Prerequisites

- A C++17‑compatible compiler (e.g., `clang++`, `g++`, MSVC).
- **CMake** ≥ 3.12.
- `nlohmann_json` library (available via Homebrew `brew install nlohmann-json`, apt `libnlohmann-json-dev`, vcpkg, etc.).
- POSIX socket support (Linux/macOS). Windows support would need additional work.

## Build & Install

### Using CMake (recommended)

```bash
git clone https://github.com/myshy13/cpphttp.git
cd cpphttp
mkdir build && cd build
cmake ..            # configure the project
cmake --build .     # builds the `cpphttp` executable
```

The built binary `cpphttp` lives in the `build` directory.

### Using the provided Makefile

```bash
make                # compiles the library and the test binary
make test           # builds and runs the test suite
```

The Makefile compiles all `*.cpp` files (except `src/main.cpp`) into a temporary test executable `testout`, runs it, and then removes it.

## Running the Example Server

The repository ships a minimal example in `src/main.cpp`. After a successful CMake build:

```bash
./build/cpphttp
```

You should see output similar to:

```
listening on: http://localhost:8080
```

You can then interact with the server, for example:

```bash
curl http://localhost:8080/
# → This is my c++ server!
```

## API Reference (quick cheat‑sheet)

### Core Types (`include/server.hpp`)

| Type | Description |
|------|-------------|
| `Server` | Main server object. Create with `server::createServer(port)`. |
| `Router` | Container for routes that can be mounted on a `Server` via `server.use(router)`. |
| `Request` | Represents an incoming HTTP request. Contains method, path, headers, params, body, IP, content type, and a `PluginContext`. |
| `Response` | Mutable response builder. Methods: `status(int)`, `setHeader(key,value)`, `send(string)`, `sendFile(path)`. |
| `ServerPlugin` | Base class for plugins. Override `onInit`, `onBeforeRequest`, `onAfterRequest`, `onShutdown`. |
| `PluginContext` | Simple key/value store attached to each request; supports `set`, `get`, and `get_mutable`. |
| `Middleware` | Struct holding a prefix, allowed methods and a handler function inserted via `Server::use`. |
| `Cors` | Holds CORS configuration for a path prefix. |

### Route Registration (methods on `Server` and `Router`)

```cpp
server.get("/", handler);
server.post("/login", handler);
server.patch("/resource/:id", handler);
server.delete_("/resource/:id", handler);
server.put("/resource/:id", handler);
server.head("/ping", handler);
server.options("/resource", handler);
```

All methods accept a `Handler` – a `std::function<void(Request&,Response&)>`.

### Parameterised Routes

```cpp
server.get("/users/:id", [](Request& req, Response& res) {
    std::string userId = req.params["id"]; // extracted from the URL
    res.status(200).send("User " + userId);
});
```

Route matching and parameter extraction are performed by `isStructuralMatch` and `getParams` in `server.hpp`.

### Middleware

```cpp
app.use("/auth", Method::ALL,
    [](Request& req, Response&, NextHandler next) {
        // Example: set a value in the request's PluginContext
        req.context.set("user", "anonymous");
        next(req, res); // continue to the next middleware / route
    });
```

Only middleware whose `prefix` matches the request path and whose `allowedMethods` contain the request method will be invoked.

### CORS

```cpp
app.cors("/", "*", Method::ALL); // allow any origin on all routes
```

### Static File Serving

```cpp
app.staticDir("/public", "./static"); // serves files under ./static when URL starts with /public
```

MIME types are inferred from the filename extension (see `contentTypeFromExtension`).

### Plugins

Create a class derived from `ServerPlugin` and register it with the server:

```cpp
class MyPlugin : public ServerPlugin {
public:
    void onBeforeRequest(Request& req, Response&) override {
        // custom logic before the request is routed
    }
    void onAfterRequest(Request& req, Response&) override {
        // custom logic after the response has been generated
    }
};

app.registerPlugin(std::make_unique<MyPlugin>());
```

Plugins receive the same `prefix` and `allowedMethods` filtering as middleware.

## Testing

The test suite lives in `test/test.cpp`. It demonstrates:

- Basic route handling and response codes.
- Parameter extraction (`/echo/:key/:value`).
- CORS header handling.
- Middleware that writes to the request `PluginContext`.
- A custom `TestPlugin` that validates context values.
- Router mounting (`app.use(router)`).

Run the tests with the Makefile target:

```bash
make test
```

The program prints `[PASS]`/`[FAIL]` messages and exits with status 0 when all checks succeed.

## Limitations & Production Notice

- **Not production‑ready** – the library is deliberately simple and lacks many production‑grade features (HTTPS, request time‑outs, robust error handling, HTTP/2, etc.).
- Only POSIX sockets are used; Windows support would need additional work.
- No built‑in request body size limits or multipart parsing.
- Concurrency is limited to a fixed‑size thread pool (`ThreadPool pool{4}` in `Server`).

Feel free to experiment, extend and improve the library.

## License

`cpphttp` is distributed under the MIT License. See the `LICENSE` file for details.

## Contributing

Contributions are welcome! Typical workflow:

1. Fork the repository.
2. Create a feature branch.
3. Add tests for any new behaviour.
4. Open a pull request.

---

For deeper insight, explore the header `include/server.hpp` and the source files in `src/`.