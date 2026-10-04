// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.
// Opt-in research of a shareable accepted socket on an A11-derived worker.

#include <array>
#include <chrono>
#include <cstdint>
#include <utility>

#include "symbian/api/connectivity/active_tcp_listener.h"
#include "symbian/concurrency/worker_executor.h"

extern "C" void ProbeStartScheduler();

namespace {

class Observer final : public symbian::api::connectivity::TcpAcceptObserver {
 public:
  void OnAccept(
      absl::StatusOr<symbian::api::connectivity::TcpClient> result) override {
    if (!result.ok()) {
      return;
    }
    auto posted = worker.Post([client = std::move(*result)]() mutable {
      const std::array<std::uint8_t, 1> marker{'W'};
      if (!client.SendFor(marker, std::chrono::seconds(5)).ok()) {
        return;
      }
      std::array<std::uint8_t, 1> request{};
      auto received = client.ReceiveFor(request, std::chrono::seconds(5));
      if (!received.ok() || *received != 1 || request[0] != 'Q') {
        return;
      }
      const std::array<std::uint8_t, 1> response{'A'};
      client.SendFor(response, std::chrono::seconds(5));
    });
    if (!posted.ok()) {
      return;
    }
    listener->AcceptNext();
  }

  symbian::api::connectivity::ActiveTcpListener* listener = nullptr;
  symbian::concurrency::WorkerExecutor worker{2};
};

}  // namespace

extern "C" int RunActiveProbe() {
  Observer observer;
  symbian::api::connectivity::ActiveTcpListener listener(observer);
  observer.listener = &listener;
  if (!listener.EnableWorkerSharing().ok() ||
      !listener.ListenIpv4({127, 0, 0, 1}, 39100).ok()) {
    return -236;
  }
  ProbeStartScheduler();
  return 0;
}
