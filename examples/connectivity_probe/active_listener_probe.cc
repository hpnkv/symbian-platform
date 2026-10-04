// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include <array>
#include <cstdint>

#include "symbian/api/connectivity/active_tcp_listener.h"

extern "C" void ProbeStartScheduler();
extern "C" void ProbeStopScheduler();

namespace {

class Observer final : public symbian::api::connectivity::TcpAcceptObserver {
 public:
  void OnAccept(
      absl::StatusOr<symbian::api::connectivity::TcpClient> result) override {
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

  symbian::api::connectivity::ActiveTcpListener* listener = nullptr;
  int accepted_count = 0;
  int outcome = 0;
};

}  // namespace

extern "C" int RunActiveProbe() {
  Observer observer;
  symbian::api::connectivity::ActiveTcpListener listener(observer);
  observer.listener = &listener;
  if (!listener.ListenIpv4({127, 0, 0, 1}, 39099).ok()) {
    return -230;
  }
  ProbeStartScheduler();
  return observer.accepted_count == 2 ? observer.outcome : -235;
}
