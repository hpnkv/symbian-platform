// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "symbian/api/connectivity/tcp_listener.h"

#include <utility>

#include "native_tcp_client.h"
#include "symbian/native_status.h"

namespace symbian::api::connectivity {

absl::StatusOr<TcpListener> TcpListener::ListenIpv4(
    std::array<std::uint8_t, 4> address, std::uint16_t port) {
  if (port == 0) {
    return absl::InvalidArgumentError("TCP listener port must be nonzero");
  }
  const unsigned packed = (static_cast<unsigned>(address[0]) << 24) |
                          (static_cast<unsigned>(address[1]) << 16) |
                          (static_cast<unsigned>(address[2]) << 8) |
                          static_cast<unsigned>(address[3]);
  NativeTcpListener* native = nullptr;
  const int result = SymbianDeviceTcpListen(packed, port, &native);
  if (result != 0) {
    return symbian::StatusFromNativeError(result, "Listen on TCP socket");
  }
  return TcpListener(native);
}

TcpListener::TcpListener(TcpListener&& other) noexcept
    : native_(std::exchange(other.native_, nullptr)) {}

TcpListener& TcpListener::operator=(TcpListener&& other) noexcept {
  if (this != &other) {
    SymbianDeviceTcpListenerClose(native_);
    native_ = std::exchange(other.native_, nullptr);
  }
  return *this;
}

TcpListener::~TcpListener() {
  SymbianDeviceTcpListenerClose(native_);
}

absl::StatusOr<TcpClient> TcpListener::Accept() {
  if (native_ == nullptr) {
    return absl::FailedPreconditionError("TCP listener is closed");
  }
  NativeTcpClient* client = nullptr;
  const int result = SymbianDeviceTcpAccept(native_, &client);
  if (result != 0) {
    return symbian::StatusFromNativeError(result, "Accept TCP stream");
  }
  return TcpClient(client);
}

absl::StatusOr<TcpClient> TcpListener::AcceptFor(
    std::chrono::milliseconds timeout) {
  if (native_ == nullptr) {
    return absl::FailedPreconditionError("TCP listener is closed");
  }
  if (timeout.count() < 0 || timeout.count() > 60000) {
    return absl::InvalidArgumentError("TCP accept timeout must be 0-60000 ms");
  }
  NativeTcpClient* client = nullptr;
  const int result = SymbianDeviceTcpAcceptFor(
      native_, static_cast<int>(timeout.count()), &client);
  if (result != 0) {
    return symbian::StatusFromNativeError(result, "Accept TCP stream");
  }
  return TcpClient(client);
}

}  // namespace symbian::api::connectivity
