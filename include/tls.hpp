#pragma once
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <string>

class TLSContext {
public:
  TLSContext(const std::string &certFile, const std::string &keyFile);
  ~TLSContext();

  /** Create a new SSL* for an accepted socket fd. */
  SSL *makeSSL(int clientFd) const;

  /** Perform the handshake (blocking). Returns true on success. */
  static bool doHandshake(SSL *ssl);

  // Disallow copy – we own OpenSSL objects.
  TLSContext(const TLSContext &) = delete;
  TLSContext &operator=(const TLSContext &) = delete;

private:
  SSL_CTX *ctx_ = nullptr;
};