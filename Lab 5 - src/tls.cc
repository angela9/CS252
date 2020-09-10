/**
 * This file contains your implementation of a TLS socket and socket acceptor. The TLS socket uses
 * the OpenSSL library to handle all socket communication, so you need to configure OpenSSL and use the
 * OpenSSL functions to read/write to the socket. src/tcp.cc is provided for your reference on 
 * Sockets and SocketAdaptors and examples/simple_tls_server.c is provided for your reference on OpenSSL.
 */

#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include <iostream>
#include <sstream>
#include <cstring>
#include <memory>

#include "tls.hh"
#include "errors.hh"


TLSSocket::TLSSocket(int port_no, struct sockaddr_in addr, SSL* ssl) :
  _socket(port_no), _addr(addr), _ssl(ssl) {
    // TODO: Task 2.1
  char inet_pres[INET_ADDRSTRLEN];
  if (inet_ntop(addr.sin_family, &(addr.sin_addr), inet_pres, INET_ADDRSTRLEN)) {
    std::cout << "Received a connection from " << inet_pres << std::endl;
  }
}
TLSSocket::~TLSSocket() noexcept {
    // TODO: Task 2.1
  std::cout << "Closing TLS socket fd " << _socket;
  char inet_pres[INET_ADDRSTRLEN];
  if (inet_ntop(_addr.sin_family, &(_addr.sin_addr), inet_pres, INET_ADDRSTRLEN)) {
    std::cout << " from " << inet_pres;
  }
  std::cout << std::endl;
  SSL_free(_ssl);
  close(_socket);
}

char TLSSocket::getc() {
    // TODO: Task 2.1
    char c = '\0';
    int i = SSL_read(_ssl, &c, 1);
    if (i < 0) {
	throw ConnectionError("Unable to read a character");
    }
    else if (i == 0) {
	c = EOF;
    }
    return c;
}

ssize_t TLSSocket::read(char *buf, size_t buf_len) {
    // TODO: Task 2.1
    ssize_t r = 0;
    SSL_read(_ssl, buf, buf_len);
    return r;
}

std::string TLSSocket::readline() {
    std::string str;
    char c;
    while ((c = getc()) != '\n' && c != EOF) {
        str.append(1, c);
    }
    if (c == '\n') {
        str.append(1, '\n');
    }
    return str;
}

void TLSSocket::write(std::string const &str) {
    write(str.c_str(), str.length());
}

void TLSSocket::write(char const *const buf, const size_t buf_len) {
    if (buf == NULL)
        return;
    // TODO: Task 2.1
    SSL_write(_ssl, buf, buf_len);
}

void cleanup_openssl() {
  EVP_cleanup();
}

void init_openssl() {
  SSL_load_error_strings();
  OpenSSL_add_ssl_algorithms();
}

void configure_context(SSL_CTX *ctx) {
  SSL_CTX_set_ecdh_auto(ctx, 1);
  if (SSL_CTX_use_certificate_file(ctx, "cert.pem", SSL_FILETYPE_PEM) <= 0) {
    ERR_print_errors_fp(stderr);
  }
  if (SSL_CTX_use_PrivateKey_file(ctx, "key.pem", SSL_FILETYPE_PEM) <= 0) {
    ERR_print_errors_fp(stderr);
  }
}

SSL_CTX *create_context() {
  const SSL_METHOD *method;
  method = SSLv23_server_method();
  SSL_CTX *ctx;
  ctx = SSL_CTX_new(method);
  if (!ctx) {
    ERR_print_errors_fp(stderr);
    throw ConnectionError("Unable to create SSL context");
  }
  return ctx;
}

TLSSocketAcceptor::TLSSocketAcceptor(const int portno) {
    // TODO: Task 2.1
    init_openssl();
    _ssl_ctx = create_context();
    configure_context(_ssl_ctx);
    _addr.sin_family = AF_INET;
    _addr.sin_port = htons(portno);
    _addr.sin_addr.s_addr = htonl(INADDR_ANY);
    _master_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (_master_socket < 0) {
	throw ConnectionError("Unable to create socket: " + std::string(strerror(errno)));
    }
    if (bind(_master_socket, (struct sockaddr*)&_addr, sizeof(_addr)) < 0) {
	throw ConnectionError("Unable to bind to socket: " + std::string(strerror(errno)));
    }
    if (listen(_master_socket, 1) < 0) {
	throw ConnectionError("Uniable to listen to socket: " + std::string(strerror(errno)));
    }
}

Socket_t TLSSocketAcceptor::accept_connection() const {
    // TODO: Task 2.1
  struct sockaddr_in addr;
  socklen_t addr_len = sizeof(addr);
  int acc = accept(_master_socket, (struct sockaddr*)&addr, &addr_len);
  if (acc == -1) {
    throw ConnectionError("Unable to accept connection: " + std::string(strerror(errno)));
  }
  SSL *ssl;
  ssl = SSL_new(_ssl_ctx);
  SSL_set_fd(ssl, acc);
  if (SSL_accept(ssl) <= 0) {
    ERR_print_errors_fp(stderr);
  }
  return std::make_unique<TLSSocket>(acc, addr, ssl); 
}

TLSSocketAcceptor::~TLSSocketAcceptor() noexcept {
    // TODO: Task 2.1
    std::cout << "Closing socket " << _master_socket << std::endl;
    close(_master_socket);
}
