// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.
#ifndef SYMBIAN_API_CONNECTIVITY_WEBSOCKET_H_
#define SYMBIAN_API_CONNECTIVITY_WEBSOCKET_H_
#include <memory>
#include <span>
#include <string>
#include <utility>

#include "absl/time/time.h"
#include "symbian/api/connectivity/byte_stream.h"
#include "symbian/api/connectivity/tcp_listener.h"
#include "symbian/websocket/websocket.h"

namespace symbian::api::connectivity {
/** @brief Synchronous worker-owned WebSocket using the SDK TCP runtime.
 *
 * Accept and Connect handshake an already connected socket; absolute deadlines
 * bound each operation. All calls and destruction stay on its owner thread.
 * Receive exposes a byte stream across binary messages for WireStream framing.
 * Close sends the RFC 6455 close and drains the peer reply by its deadline.
 * No scheduler is created: use the SDK WorkerExecutor for background work.
 */
class WebSocketStream {
 public:
  static absl::StatusOr<WebSocketStream> Connect(
      TcpClient client, websocket::Options options,
      absl::Time deadline = absl::InfiniteFuture());
  static absl::StatusOr<WebSocketStream> Accept(
      TcpClient client, websocket::Options options = {},
      absl::Time deadline = absl::InfiniteFuture());
  static absl::StatusOr<WebSocketStream> Connect(
      net::ByteStream transport, websocket::Options options,
      absl::Time deadline = absl::InfiniteFuture());
  static absl::StatusOr<WebSocketStream> Accept(
      net::ByteStream transport, websocket::Options options = {},
      absl::Time deadline = absl::InfiniteFuture());
  WebSocketStream(WebSocketStream&&) noexcept = default;
  WebSocketStream& operator=(WebSocketStream&&) noexcept = default;
  absl::Status Send(std::span<const std::uint8_t> bytes,
                    absl::Time deadline = absl::InfiniteFuture());
  absl::StatusOr<std::size_t> Receive(
      std::span<std::uint8_t> bytes,
      absl::Time deadline = absl::InfiniteFuture());
  absl::Status Close(absl::Time deadline);
  void Abort();

 private:
  WebSocketStream(net::ByteStream transport,
                  std::unique_ptr<websocket::WebSocket> codec);
  static absl::StatusOr<WebSocketStream> Open(net::ByteStream transport,
                                              websocket::Role role,
                                              websocket::Options options,
                                              absl::Time deadline);
  absl::Status Flush(absl::Time deadline);
  absl::Status Pump(absl::Time deadline);
  net::ByteStream transport_;
  std::unique_ptr<websocket::WebSocket> codec_;
  std::string pending_;
  std::size_t offset_ = 0;
};

/** @brief Reusable bounded IPv4 WebSocket listener on a worker thread. */
class WebSocketServer {
 public:
  static absl::StatusOr<WebSocketServer> ListenIpv4(
      std::array<std::uint8_t, 4> address, std::uint16_t port,
      websocket::Options options = {});
  absl::StatusOr<WebSocketStream> Accept(absl::Time deadline);

 private:
  WebSocketServer(TcpListener listener, websocket::Options options)
      : listener_(std::move(listener)), options_(std::move(options)) {}

  TcpListener listener_;
  websocket::Options options_;
};
}  // namespace symbian::api::connectivity
#endif
