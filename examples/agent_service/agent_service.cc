// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.
// Manually started, loopback-only, emulator research agent. The bundled
// certificate is a public test fixture and has no device security value.

#include <array>
#include <chrono>
#include <cstdint>
#include <span>
#include <string_view>
#include <utility>

#include "agent_certificates.h"
#include "symbian/agent/guest_control.h"
#include "symbian/api/connectivity/active_tcp_listener.h"
#include "symbian/api/connectivity/tls_server.h"
#include "symbian/concurrency/worker_executor.h"

extern "C" void ProbeStartScheduler();

namespace {

using symbian::api::connectivity::TcpClient;
using symbian::api::connectivity::TlsServer;
using namespace std::chrono_literals;

constexpr std::uint16_t kAgentPort = 39101;
constexpr std::size_t kMaximumFrame = 4096;
constexpr std::size_t kMaximumRequestsPerConnection = 16;
constexpr std::size_t kTlsWorkerStackBytes = 256 * 1024;

bool ReadExactly(TlsServer& server, std::span<std::uint8_t> output) {
  while (!output.empty()) {
    auto received = server.ReadFor(output, 5s);
    if (!received.ok() || *received == 0) {
      return false;
    }
    output = output.subspan(*received);
  }
  return true;
}

bool WriteExactly(TlsServer& server, std::span<const std::uint8_t> input) {
  // TlsServer::WriteFor writes the entire input or returns an error.
  return server.WriteFor(input, 5s).ok();
}

void Serve(TcpClient client) {
  auto server =
      TlsServer::Create(k_server_cert, k_server_key, k_server_cert,
                        symbian::api::connectivity::TlsVersion::kTls13);
  if (!server.ok() || !server->Accept(std::move(client), 10s).ok()) {
    return;
  }

  for (std::size_t request_index = 0;
       request_index < kMaximumRequestsPerConnection; ++request_index) {
    std::array<std::uint8_t, 4> prefix{};
    if (!ReadExactly(*server, prefix)) {
      return;
    }
    const std::uint32_t length = (static_cast<std::uint32_t>(prefix[0]) << 24) |
                                 (static_cast<std::uint32_t>(prefix[1]) << 16) |
                                 (static_cast<std::uint32_t>(prefix[2]) << 8) |
                                 static_cast<std::uint32_t>(prefix[3]);
    if (length == 0 || length > kMaximumFrame) {
      return;
    }
    std::array<std::uint8_t, kMaximumFrame> payload{};
    if (!ReadExactly(*server, std::span(payload).first(length))) {
      return;
    }
    auto request = symbian::agent::ParseGuestControl(std::string_view(
        reinterpret_cast<const char*>(payload.data()), length));
    if (!request.ok()) {
      return;
    }
    auto response = symbian::agent::PackGuestResult(*request);
    if (!response.ok()) {
      return;
    }
    const auto count = static_cast<std::uint32_t>(response->size());
    const std::array<std::uint8_t, 4> response_prefix{
        static_cast<std::uint8_t>(count >> 24),
        static_cast<std::uint8_t>(count >> 16),
        static_cast<std::uint8_t>(count >> 8),
        static_cast<std::uint8_t>(count)};
    if (!WriteExactly(*server, response_prefix) ||
        !WriteExactly(*server, std::span(reinterpret_cast<const std::uint8_t*>(
                                             response->data()),
                                         response->size()))) {
      return;
    }
  }
}

class AgentService final
    : public symbian::api::connectivity::TcpAcceptObserver {
 public:
  AgentService() : listener_(*this) {}

  absl::Status Start() {
    absl::Status status = listener_.EnableWorkerSharing();
    if (!status.ok()) {
      return status;
    }
    return listener_.ListenIpv4({127, 0, 0, 1}, kAgentPort);
  }

  void OnAccept(absl::StatusOr<TcpClient> result) override {
    if (result.ok()) {
      // The bounded queue closes a rejected client through its captured owner.
      worker_.PostFiber(
          [client = std::move(*result)]() mutable { Serve(std::move(client)); },
          kTlsWorkerStackBytes);
    }
    listener_.AcceptNext();
  }

 private:
  symbian::concurrency::WorkerExecutor worker_{4};
  symbian::api::connectivity::ActiveTcpListener listener_;
};

}  // namespace

extern "C" int RunActiveProbe() {
  AgentService service;
  if (!service.Start().ok()) {
    return -301;
  }
  ProbeStartScheduler();
  return 0;
}
