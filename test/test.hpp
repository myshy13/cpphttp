#pragma once

#include "colors.hpp"
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <netdb.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

static std::string sendRawRequest(int port, const std::string &request) {
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  addrinfo *result = nullptr;
  int rc =
      getaddrinfo("127.0.0.1", std::to_string(port).c_str(), &hints, &result);
  if (rc != 0 || result == nullptr) {
    return "getaddrinfo failed";
  }

  int sock =
      socket(result->ai_family, result->ai_socktype, result->ai_protocol);
  if (sock < 0) {
    freeaddrinfo(result);
    return "socket failed";
  }

  if (connect(sock, result->ai_addr, result->ai_addrlen) != 0) {
    close(sock);
    freeaddrinfo(result);
    return "connect failed";
  }

  freeaddrinfo(result);

  ssize_t sent = send(sock, request.data(), request.size(), 0);
  if (sent < 0) {
    close(sock);
    return "send failed";
  }

  std::string response;
  char buffer[4096];
  while (true) {
    ssize_t received = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (received <= 0) {
      break;
    }
    buffer[received] = '\0';
    response += buffer;
  }

  close(sock);
  return response;
}

static std::string makeRequest(
    int port, const std::string &method, const std::string &target,
    const std::string &body = "",
    const std::vector<std::pair<std::string, std::string>> &headers = {}) {
  std::string request = method + " " + target + " HTTP/1.1\r\n";
  request += "Host: localhost\r\n";

  for (auto &header : headers) {
    request += header.first + ": " + header.second + "\r\n";
  }

  if (!body.empty()) {
    request += "Content-Length: " + std::to_string(body.size()) + "\r\n";
  }

  request += "Connection: close\r\n\r\n";
  request += body;
  return sendRawRequest(port, request);
}

static bool checkResponse(const std::string &response,
                          const std::string &expected, const char *label) {
  bool passed = response.find(expected) != std::string::npos;
  std::cout << (passed ? Colors::BRIGHT_GREEN + "[PASS] "
                       : Colors::BRIGHT_RED + "[FAIL] ")
            << Colors::NC << label << std::endl;
  if (!passed) {
    std::cout << "  Expected substring: " << expected << std::endl;
    std::cout << "  Actual response: " << response << std::endl;
  }
  return passed;
}