// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.
// Opt-in research of a shareable accepted socket on an A11-derived worker.

#include <array>
#include <cstdint>
#include <utility>

#include <absl/base/nullability.h>

#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "symbian/api/connectivity/active_tcp_listener.h"
#include "symbian/concurrency/worker_executor.h"

extern "C" void ProbeStartScheduler();

namespace {

class ProbeState final {
 public:
  void OnAccept(absl::StatusOr<symbian::api::connectivity::TcpClient> result) {
    if (!result.ok()) {
      return;
    }
    auto posted = worker.Post([client = std::move(*result)]() mutable {
      const std::array<std::uint8_t, 1> marker{'W'};
      if (!client.Send(marker, absl::Now() + absl::Minutes(1)).ok()) {
        return;
      }
      std::array<std::uint8_t, 1> request{};
      auto received = client.Receive(request, absl::Now() + absl::Minutes(1));
      if (!received.ok() || *received != 1 || request[0] != 'Q') {
        return;
      }
      const std::array<std::uint8_t, 1> response{'A'};
      if (!client.Send(response, absl::Now() + absl::Minutes(1)).ok()) {
        return;
      }
    });
    if (!posted.ok()) {
      return;
    }
    if (!listener->AcceptNext().ok()) {
      return;
    }
  }

  symbian::api::connectivity::ActiveTcpListener* absl_nullable listener =
      nullptr;
  symbian::concurrency::WorkerExecutor worker{2};
};

}  // namespace

extern "C" int RunActiveProbe() {
  ProbeState state;
  symbian::api::connectivity::ActiveTcpListener listener(
      [&state](auto result) { state.OnAccept(std::move(result)); });
  state.listener = &listener;
  if (!listener.EnableWorkerSharing().ok() ||
      !listener.ListenIpv4({127, 0, 0, 1}, 39100).ok()) {
    return -236;
  }
  ProbeStartScheduler();
  return 0;
}
