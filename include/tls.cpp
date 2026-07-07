#include "tls.hpp"
#include <iostream>
#include <filesystem>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>

TLSContext::TLSContext(const std::string &certFile,
                       const std::string &keyFile) {
  SSL_load_error_strings();
  OpenSSL_add_ssl_algorithms();

  const SSL_METHOD *method = TLS_server_method(); // TLS 1.2+1.3
  ctx_ = SSL_CTX_new(method);
  if (!ctx_) {
    unsigned long err = ERR_get_error();
    std::string errStr = err ? ERR_error_string(err, nullptr) : "Unknown error";
    throw std::runtime_error("Failed to create SSL_CTX: " + errStr);
  }

  // Restrict to TLS 1.2 & 1.3 (disable SSLv2/3)
  SSL_CTX_set_min_proto_version(ctx_, TLS1_2_VERSION);
  SSL_CTX_set_max_proto_version(ctx_, TLS1_3_VERSION);

  std::filesystem::path certPath = std::filesystem::absolute(certFile);
  std::filesystem::path keyPath = std::filesystem::absolute(keyFile);
  std::cerr << "Loading certificate from: " << certPath << std::endl;
  std::cerr << "Loading private key from: " << keyPath << std::endl;
  if (SSL_CTX_use_certificate_file(ctx_, certPath.c_str(), SSL_FILETYPE_PEM) <= 0) {
    unsigned long err = ERR_get_error();
    std::string errStr = err ? ERR_error_string(err, nullptr) : "Unknown error";
    throw std::runtime_error("Failed to load certificate: " + errStr);
  }

  if (SSL_CTX_use_PrivateKey_file(ctx_, keyPath.c_str(), SSL_FILETYPE_PEM) <= 0) {
    unsigned long err = ERR_get_error();
    std::string errStr = err ? ERR_error_string(err, nullptr) : "Unknown error";
    throw std::runtime_error("Failed to load private key: " + errStr);
  }

  if (!SSL_CTX_check_private_key(ctx_)) {
    throw std::runtime_error("Private key does not match certificate");
  }

  // Verify that the loaded certificate is trusted by the system's CA store.
  // This will emit a warning if verification fails (e.g., self‑signed or unknown CA).
  {
    const X509 *cert = SSL_CTX_get0_certificate(ctx_);
    if (cert) {
      X509_STORE *store = X509_STORE_new();
      if (store) {
        // Load default system CA paths (e.g., /etc/ssl/certs on Linux, Keychain on macOS).
        X509_STORE_set_default_paths(store);
        X509_STORE_CTX *verify_ctx = X509_STORE_CTX_new();
        if (verify_ctx && X509_STORE_CTX_init(verify_ctx, store, const_cast<X509 *>(cert), nullptr) == 1) {
          if (X509_verify_cert(verify_ctx) != 1) {
            std::cerr << "WARNING: Loaded TLS certificate is not trusted by the system CA store."
                      << std::endl;
          }
        }
        if (verify_ctx) X509_STORE_CTX_free(verify_ctx);
        X509_STORE_free(store);
      }
    }
  }
}

TLSContext::~TLSContext() {
  if (ctx_)
    SSL_CTX_free(ctx_);
  EVP_cleanup();
  ERR_free_strings();
}

SSL *TLSContext::makeSSL(int clientFd) const {
  SSL *ssl = SSL_new(ctx_);
  SSL_set_fd(ssl, clientFd);
  return ssl;
}

bool TLSContext::doHandshake(SSL *ssl) {
  int ret = SSL_accept(ssl);
  if (ret <= 0) {
    int ssl_err = SSL_get_error(ssl, ret);
    if (ssl_err == SSL_ERROR_SSL) {
      unsigned long err_code = ERR_get_error();
      const char *err_str = (err_code != 0) ? ERR_error_string(err_code, nullptr) : "Unknown SSL error";
      std::cerr << "TLS handshake error: " << err_str << std::endl;
    } else {
      std::cerr << "TLS handshake failed with SSL error code: " << ssl_err << std::endl;
    }
    return false;
  }
  return true;
}
