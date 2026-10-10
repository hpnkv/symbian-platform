// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.
#ifndef SYMBIAN_API_CONNECTIVITY_HTTP_H_
#define SYMBIAN_API_CONNECTIVITY_HTTP_H_
#include <absl/status/status_macros.h>
#include "symbian/api/connectivity/byte_stream.h"
#include "symbian/api/connectivity/tcp_listener.h"
#include "symbian/http/connection.h"

namespace symbian::api::connectivity {
/** @brief Native HTTP client over the existing worker-owned TCP runtime.
 *
 * An explicit address separates routing from the HTTP authority/TLS identity.
 * For HTTPS, establish TlsStream::Connect then pass it to Connection::Client;
 * the negotiated ALPN must match the selected HTTP protocol.
 */
class HttpClient {
 public:
  static absl::StatusOr<std::unique_ptr<http::Connection>> ConnectHost(
      std::string_view hostname, std::uint16_t port, http::RequestHead request,
      absl::Time deadline = absl::InfiniteFuture(),
      http::Protocol protocol = http::Protocol::kHttp11,
      http::Limits limits = {}, std::optional<std::size_t> body_length = 0) {
    if (request.scheme != "http") {
      return absl::InvalidArgumentError("HTTPS requires a verified TLS stream");
    }
    ABSL_ASSIGN_OR_RETURN(auto tcp, TcpClient::ConnectHost(hostname, port, deadline));
    return http::Connection::Client(
        std::make_unique<TcpByteStream>(std::move(tcp)), std::move(request),
        protocol, limits, body_length, deadline);
  }

  static absl::StatusOr<std::unique_ptr<http::Connection>> ConnectIpv4(
      std::array<std::uint8_t, 4> address, std::uint16_t port,
      http::RequestHead request,
      http::Protocol protocol = http::Protocol::kHttp11,
      absl::Time deadline = absl::InfiniteFuture(), http::Limits limits = {},
      std::optional<std::size_t> body_length = 0) {
    if (request.scheme != "http") {
      return absl::InvalidArgumentError("HTTPS requires a verified TLS stream");
    }
    ABSL_ASSIGN_OR_RETURN(auto tcp, TcpClient::ConnectIpv4(address, port, deadline));
    return http::Connection::Client(
        std::make_unique<TcpByteStream>(std::move(tcp)), std::move(request),
        protocol, limits, body_length, deadline);
  }
};

/** @brief Native bounded HTTP listener; each accepted exchange owns its socket. */
class HttpServer {
 public:
  static absl::StatusOr<HttpServer> ListenIpv4(
      std::array<std::uint8_t, 4> address, std::uint16_t port,
      http::Protocol protocol = http::Protocol::kHttp11,
      http::Limits limits = {}) {
    ABSL_ASSIGN_OR_RETURN(auto listener, TcpListener::ListenIpv4(address, port));
    return HttpServer(std::move(listener), protocol, limits);
  }

  absl::StatusOr<std::unique_ptr<http::Connection>> Accept(
      absl::Time deadline) {
    ABSL_ASSIGN_OR_RETURN(auto tcp, listener_.Accept(deadline));
    return http::Connection::Accept(
        std::make_unique<TcpByteStream>(std::move(tcp)), protocol_, limits_,
        deadline);
  }

 private:
  HttpServer(TcpListener listener, http::Protocol protocol, http::Limits limits)
      : listener_(std::move(listener)), protocol_(protocol), limits_(limits) {}

  TcpListener listener_;
  http::Protocol protocol_;
  http::Limits limits_;
};
}  // namespace symbian::api::connectivity
#endif
