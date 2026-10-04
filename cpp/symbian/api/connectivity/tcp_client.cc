// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "symbian/api/connectivity/tcp_client.h"

#include <utility>

#include "native_tcp_client.h"
#include "symbian/native_status.h"

namespace symbian::api::connectivity {
namespace {

constexpr std::size_t kMaximumOperationBytes = 32 * 1024;

bool ValidTimeout(std::chrono::milliseconds timeout) {
  return timeout.count() >= 0 && timeout.count() <= 60000;
}

}  // namespace

absl::StatusOr<TcpClient> TcpClient::ConnectIpv4(
    std::array<std::uint8_t, 4> address, std::uint16_t port) {
  if (port == 0) {
    return absl::InvalidArgumentError("TCP port must be nonzero");
  }
  const unsigned packed = (static_cast<unsigned>(address[0]) << 24) |
                          (static_cast<unsigned>(address[1]) << 16) |
                          (static_cast<unsigned>(address[2]) << 8) |
                          static_cast<unsigned>(address[3]);
  NativeTcpClient* native = nullptr;
  const int result = SymbianDeviceTcpConnect(packed, port, &native);
  if (result != 0) {
    return symbian::StatusFromNativeError(result, "Connect TCP socket");
  }
  return TcpClient(native);
}

TcpClient::TcpClient(TcpClient&& other) noexcept
    : native_(std::exchange(other.native_, nullptr)) {}

TcpClient& TcpClient::operator=(TcpClient&& other) noexcept {
  if (this != &other) {
    SymbianDeviceTcpClose(native_);
    native_ = std::exchange(other.native_, nullptr);
  }
  return *this;
}

TcpClient::~TcpClient() {
  SymbianDeviceTcpClose(native_);
}

absl::Status TcpClient::Send(std::span<const std::uint8_t> bytes) {
  if (native_ == nullptr) {
    return absl::FailedPreconditionError("TCP socket is closed");
  }
  if (bytes.size() > kMaximumOperationBytes) {
    return absl::ResourceExhaustedError("TCP send exceeds 32 KiB");
  }
  if (bytes.empty()) {
    return absl::OkStatus();
  }
  const int result = SymbianDeviceTcpSend(native_, bytes.data(),
                                          static_cast<int>(bytes.size()));
  return result == 0 ? absl::OkStatus()
                     : symbian::StatusFromNativeError(result, "Send TCP data");
}

absl::Status TcpClient::SendFor(std::span<const std::uint8_t> bytes,
                                std::chrono::milliseconds timeout) {
  if (!ValidTimeout(timeout)) {
    return absl::InvalidArgumentError("TCP send timeout must be 0-60000 ms");
  }
  if (native_ == nullptr) {
    return absl::FailedPreconditionError("TCP socket is closed");
  }
  if (bytes.size() > kMaximumOperationBytes) {
    return absl::ResourceExhaustedError("TCP send exceeds 32 KiB");
  }
  if (bytes.empty()) {
    return absl::OkStatus();
  }
  const int result = SymbianDeviceTcpSendFor(native_, bytes.data(),
                                             static_cast<int>(bytes.size()),
                                             static_cast<int>(timeout.count()));
  return result == 0 ? absl::OkStatus()
                     : symbian::StatusFromNativeError(result, "Send TCP data");
}

absl::StatusOr<std::size_t> TcpClient::Receive(std::span<std::uint8_t> bytes) {
  if (native_ == nullptr) {
    return absl::FailedPreconditionError("TCP socket is closed");
  }
  if (bytes.size() > kMaximumOperationBytes) {
    return absl::ResourceExhaustedError("TCP receive exceeds 32 KiB");
  }
  if (bytes.empty()) {
    return std::size_t{0};
  }
  int received = 0;
  const int result = SymbianDeviceTcpReceive(
      native_, bytes.data(), static_cast<int>(bytes.size()), &received);
  if (result != 0) {
    return symbian::StatusFromNativeError(result, "Receive TCP data");
  }
  return static_cast<std::size_t>(received);
}

absl::StatusOr<std::size_t> TcpClient::ReceiveFor(
    std::span<std::uint8_t> bytes, std::chrono::milliseconds timeout) {
  if (!ValidTimeout(timeout)) {
    return absl::InvalidArgumentError("TCP receive timeout must be 0-60000 ms");
  }
  if (native_ == nullptr) {
    return absl::FailedPreconditionError("TCP socket is closed");
  }
  if (bytes.size() > kMaximumOperationBytes) {
    return absl::ResourceExhaustedError("TCP receive exceeds 32 KiB");
  }
  if (bytes.empty()) {
    return std::size_t{0};
  }
  int received = 0;
  const int result = SymbianDeviceTcpReceiveFor(
      native_, bytes.data(), static_cast<int>(bytes.size()),
      static_cast<int>(timeout.count()), &received);
  if (result != 0) {
    return symbian::StatusFromNativeError(result, "Receive TCP data");
  }
  return static_cast<std::size_t>(received);
}

}  // namespace symbian::api::connectivity
