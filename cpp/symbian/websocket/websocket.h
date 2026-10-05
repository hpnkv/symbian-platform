// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.
#ifndef SYMBIAN_WEBSOCKET_WEBSOCKET_H_
#define SYMBIAN_WEBSOCKET_WEBSOCKET_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace symbian::websocket {

enum class Role { kClient, kServer };

struct Options {
  // Required for clients; invoked only by native work on the owner thread.
  std::function<absl::StatusOr<std::array<std::uint8_t, 4>>()> mask_provider;
  std::string path = "/symbian-agent";
  std::string authority = "symbian";
  std::size_t maximum_message_bytes = 4100;
  std::size_t maximum_buffered_bytes = 65536;
};

/**
 * @brief Single-owner RFC 8441 WebSocket endpoint over nghttp2.
 *
 * Feed receives arbitrary TCP chunks; TakeOutput returns HTTP/2 bytes for
 * transport. Send/Receive preserve binary message boundaries, with bounded
 * queues and HTTP/2 flow control. Each endpoint serves one CONNECT stream.
 * The caller supplies unpredictable masking keys for every client frame.
 * This codec owns no socket, thread, Python reference or authentication policy.
 * Drain output before reading again, including after peer close. An error is
 * terminal; discard the connection after any transport failure.
 */
class WebSocket {
 public:
  static absl::StatusOr<std::unique_ptr<WebSocket>> Create(
      Role role, Options options = {});
  ~WebSocket();
  WebSocket(const WebSocket&) = delete;
  WebSocket& operator=(const WebSocket&) = delete;

  absl::Status Feed(std::string_view bytes);
  absl::StatusOr<std::string> TakeOutput();
  absl::Status Send(std::string_view message);
  absl::StatusOr<std::optional<std::string>> Receive();
  absl::Status Close();
  void Abort();
  bool open() const;
  bool closed() const;
  std::size_t buffered_amount() const;

 private:
  struct State;
  explicit WebSocket(std::unique_ptr<State> state);
  std::unique_ptr<State> state_;
};

}  // namespace symbian::websocket
#endif  // SYMBIAN_WEBSOCKET_WEBSOCKET_H_
