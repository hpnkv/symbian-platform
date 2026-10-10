// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include <array>
#include <cstdint>
#include <utility>

#include <absl/base/nullability.h>

#include "symbian/api/connectivity/active_tcp_listener.h"

extern "C" void ProbeStartScheduler();
extern "C" void ProbeStopScheduler();

namespace {

class ProbeState final {
 public:
  void OnAccept(absl::StatusOr<symbian::api::connectivity::TcpClient> result) {
    if (!result.ok()) {
      outcome = -231;
      ProbeStopScheduler();
      return;
    }
    std::array<std::uint8_t, 1> request{};
    auto received = result->Receive(request);
    if (!received.ok() || *received != 1 || request[0] != 'Q') {
      outcome = -233;
      ProbeStopScheduler();
      return;
    }
    const std::array<std::uint8_t, 1> response{'A'};
    if (!result->Send(response).ok()) {
      outcome = -234;
      ProbeStopScheduler();
      return;
    }
    if (!listener->AcceptNext().ok()) {
      outcome = -232;
      ProbeStopScheduler();
      return;
    }
    if (++accepted_count == 2) {
      ProbeStopScheduler();
    }
  }

  symbian::api::connectivity::ActiveTcpListener* absl_nullable listener =
      nullptr;
  int accepted_count = 0;
  int outcome = 0;
};

}  // namespace

extern "C" int RunActiveProbe() {
  ProbeState state;
  symbian::api::connectivity::ActiveTcpListener listener(
      [&state](auto result) { state.OnAccept(std::move(result)); });
  state.listener = &listener;
  if (!listener.ListenIpv4({127, 0, 0, 1}, 39099).ok()) {
    return -230;
  }
  ProbeStartScheduler();
  return state.accepted_count == 2 ? state.outcome : -235;
}
