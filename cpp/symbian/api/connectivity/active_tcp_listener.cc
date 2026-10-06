// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "symbian/api/connectivity/active_tcp_listener.h"

#include <utility>

#include <absl/base/nullability.h>

#include "native_tcp_client.h"
#include "symbian/native_status.h"

namespace symbian::api::connectivity {

ActiveTcpListener::ActiveTcpListener(TcpAcceptObserver* absl_nonnull observer)
    : observer_(*observer) {}

ActiveTcpListener::~ActiveTcpListener() {
  Stop();
}

absl::Status ActiveTcpListener::ListenIpv4(std::array<std::uint8_t, 4> address,
                                           std::uint16_t port) {
  if (started_) {
    return absl::FailedPreconditionError("Active TCP listener cannot restart");
  }
  if (port == 0) {
    return absl::InvalidArgumentError("Active TCP port must be nonzero");
  }
  const unsigned packed = (static_cast<unsigned>(address[0]) << 24) |
                          (static_cast<unsigned>(address[1]) << 16) |
                          (static_cast<unsigned>(address[2]) << 8) |
                          static_cast<unsigned>(address[3]);
  const int result = SymbianDeviceActiveTcpListen(
      packed, port, share_with_workers_, this,
      &ActiveTcpListener::OnNativeAccept, &native_);
  if (result != 0) {
    return symbian::StatusFromNativeError(result,
                                          "Listen on active TCP socket");
  }
  started_ = true;
  return absl::OkStatus();
}

absl::Status ActiveTcpListener::AcceptNext() {
  if (native_ == nullptr) {
    return absl::FailedPreconditionError("Active TCP listener is closed");
  }
  const int result = SymbianDeviceActiveTcpAcceptNext(native_);
  if (result != 0) {
    return symbian::StatusFromNativeError(result, "Begin active TCP accept");
  }
  return absl::OkStatus();
}

absl::Status ActiveTcpListener::EnableWorkerSharing() {
  if (started_ || native_ != nullptr) {
    return absl::FailedPreconditionError(
        "Enable worker sharing before listening");
  }
  share_with_workers_ = true;
  return absl::OkStatus();
}

void ActiveTcpListener::Stop() {
  SymbianDeviceActiveTcpClose(std::exchange(native_, nullptr));
}

bool ActiveTcpListener::is_listening() const {
  return native_ != nullptr;
}

void ActiveTcpListener::OnNativeAccept(void* absl_nonnull context,
                                       NativeTcpClient* absl_nonnull accepted,
                                       int result) {
  auto* absl_nonnull self = static_cast<ActiveTcpListener*>(context);
  if (result != 0) {
    self->observer_.OnAccept(
        symbian::StatusFromNativeError(result, "Active TCP accept"));
    return;
  }
  self->observer_.OnAccept(TcpClient(accepted));
}

}  // namespace symbian::api::connectivity
