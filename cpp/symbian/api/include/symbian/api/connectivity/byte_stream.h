// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.
#ifndef SYMBIAN_API_CONNECTIVITY_BYTE_STREAM_H_
#define SYMBIAN_API_CONNECTIVITY_BYTE_STREAM_H_
#include <absl/status/status_macros.h>
#include <algorithm>
#include <memory>

#include "symbian/api/connectivity/tcp_client.h"
#include "symbian/native_status.h"
#include "symbian/net/byte_stream.h"

namespace symbian::api::connectivity {
/** @brief Owning TCP transport for both HTTP and WebSocket streams. */
class TcpByteStream final {
 public:
  explicit TcpByteStream(TcpClient client) : client_(std::move(client)) {}

  absl::StatusOr<std::size_t> Read(std::span<std::uint8_t> bytes,
                                   absl::Time deadline) {
    auto count = client_.Receive(
        bytes.first(std::min<std::size_t>(bytes.size(), 32768)), deadline);
    // RSocket's KErrEof is an orderly receive shutdown. Disconnect/reset remains
    // an error; it must not complete an HTTP or TLS body successfully.
    if (!count.ok() && NativeErrorFromStatus(count.status()) == -25) {
      return std::size_t{0};
    }
    return count;
  }

  absl::Status Write(std::span<const std::uint8_t> bytes, absl::Time deadline) {
    while (!bytes.empty()) {
      auto count = std::min<std::size_t>(bytes.size(), 32768);
      ABSL_RETURN_IF_ERROR(client_.Send(bytes.first(count), deadline));
      bytes = bytes.subspan(count);
    }
    return absl::OkStatus();
  }

  void Close() { client_.Close(); }

 private:
  TcpClient client_;
};
}  // namespace symbian::api::connectivity
#endif
