// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "symbian/api/connectivity/tcp_client.h"

#include <utility>

#include <absl/base/nullability.h>
#include <limits.h>

#include "native_tcp_client.h"
#include "symbian/native_status.h"

namespace symbian::api::connectivity {
namespace {

constexpr std::size_t kMaximumOperationBytes = 32 * 1024;

std::int64_t NativeDeadline(absl::Time deadline) {
  return deadline == absl::InfiniteFuture() ? INT64_MAX
                                            : absl::ToUnixMicros(deadline);
}

}  // namespace

absl::StatusOr<TcpClient> TcpClient::ConnectIpv4(
    std::array<std::uint8_t, 4> address, std::uint16_t port,
    absl::Time deadline) {
  if (port == 0) {
    return absl::InvalidArgumentError("TCP port must be nonzero");
  }
  const unsigned packed = (static_cast<unsigned>(address[0]) << 24) |
                          (static_cast<unsigned>(address[1]) << 16) |
                          (static_cast<unsigned>(address[2]) << 8) |
                          static_cast<unsigned>(address[3]);
  NativeTcpClient* absl_nullable native = nullptr;
  if (const int result = SymbianDeviceTcpConnect(packed, port, &native,
                                                 NativeDeadline(deadline));
      result != 0) {
    return symbian::StatusFromNativeError(result, "Connect TCP socket");
  }
  return TcpClient(native);
}

absl::StatusOr<TcpClient> TcpClient::ConnectHost(std::string_view hostname,
                                                 std::uint16_t port,
                                                 absl::Time deadline) {
  if (hostname.empty() || hostname.size() > 253 || port == 0) {
    return absl::InvalidArgumentError("Invalid TCP hostname or port");
  }
  for (char c : hostname) {
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '-' || c == '.')) {
      return absl::InvalidArgumentError("DNS hostname requires ASCII labels");
    }
  }
  unsigned address = 0;
  if (const int result = SymbianDeviceResolveIpv4(
          hostname.data(), static_cast<int>(hostname.size()), &address,
          NativeDeadline(deadline));
      result != 0) {
    return StatusFromNativeError(result, "Resolve TCP hostname");
  }
  return ConnectIpv4({static_cast<std::uint8_t>(address >> 24),
                      static_cast<std::uint8_t>(address >> 16),
                      static_cast<std::uint8_t>(address >> 8),
                      static_cast<std::uint8_t>(address)},
                     port, deadline);
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
  Close();
}

void TcpClient::Close() {
  SymbianDeviceTcpClose(std::exchange(native_, nullptr));
}

absl::Status TcpClient::SetNoDelay(bool enabled) {
  if (native_ == nullptr) {
    return absl::FailedPreconditionError("TCP socket is closed");
  }
  return symbian::StatusFromNativeError(
      SymbianDeviceTcpSetNoDelay(native_, enabled), "Set TCP no-delay");
}

absl::Status TcpClient::Send(std::span<const std::uint8_t> bytes,
                             absl::Time deadline) {
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
                                          static_cast<int>(bytes.size()),
                                          NativeDeadline(deadline));
  return result == 0 ? absl::OkStatus()
                     : symbian::StatusFromNativeError(result, "Send TCP data");
}

absl::StatusOr<std::size_t> TcpClient::Receive(std::span<std::uint8_t> bytes,
                                               absl::Time deadline) {
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
  if (const int result = SymbianDeviceTcpReceive(
          native_, bytes.data(), static_cast<int>(bytes.size()), &received,
          NativeDeadline(deadline));
      result != 0) {
    return symbian::StatusFromNativeError(result, "Receive TCP data");
  }
  return static_cast<std::size_t>(received);
}

}  // namespace symbian::api::connectivity
