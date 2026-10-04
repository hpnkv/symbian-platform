// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.
// Manually started, loopback-only development agent. The bundled certificate
// is a public test fixture and has no physical-device security value.

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#include "agent_certificates.h"
#include "symbian/agent/guest_control.h"
#include "symbian/agent/guest_log.h"
#include "symbian/api/connectivity/active_tcp_listener.h"
#include "symbian/api/connectivity/tls_server.h"
#include "symbian/api/display/display.h"
#include "symbian/api/system/counters.h"
#include "symbian/concurrency/worker_executor.h"
#include "symbian/native_status.h"

extern "C" void ProbeStartScheduler();
extern "C" void ProbeStopScheduler();
extern "C" int AgentLocalUiMain(std::atomic<bool>* stop_requested);
extern "C" int AgentRequestForeground();

namespace {

using symbian::api::connectivity::TcpClient;
using symbian::api::connectivity::TlsServer;
using namespace std::chrono_literals;

constexpr std::uint16_t kAgentPort = 39101;
constexpr std::size_t kMaximumFrame = 4096;
constexpr std::size_t kMaximumRequestsPerConnection = 16;
constexpr std::size_t kTlsWorkerStackBytes = 256 * 1024;
constexpr auto kControlDeadline = 5s;

std::chrono::milliseconds Remaining(
    std::chrono::steady_clock::time_point deadline) {
  const auto now = std::chrono::steady_clock::now();
  if (now >= deadline) {
    return 0ms;
  }
  return std::max(1ms, std::chrono::duration_cast<std::chrono::milliseconds>(
                           deadline - now));
}

bool ReadExactly(TlsServer& server, std::span<std::uint8_t> output,
                 std::chrono::steady_clock::time_point deadline) {
  while (!output.empty()) {
    const auto remaining = Remaining(deadline);
    if (remaining == 0ms) {
      return false;
    }
    auto received = server.ReadFor(output, remaining);
    if (!received.ok() || *received == 0) {
      return false;
    }
    output = output.subspan(*received);
  }
  return true;
}

bool WriteExactly(TlsServer& server, std::span<const std::uint8_t> input,
                  std::chrono::steady_clock::time_point deadline) {
  // TlsServer::WriteFor writes the entire input or returns an error.
  const auto remaining = Remaining(deadline);
  return remaining != 0ms && server.WriteFor(input, remaining).ok();
}

symbian::agent::GuestStatusSnapshot ReadStatusSnapshot() {
  symbian::agent::GuestStatusSnapshot snapshot;
  snapshot.logs_available = true;
  auto tick = symbian::api::system::ReadTickCounter();
  if (tick.ok() && tick->period.count() > 0) {
    snapshot.tick = symbian::agent::GuestTickSnapshot{
        tick->count, static_cast<std::uint64_t>(tick->period.count())};
  }
  auto display = symbian::api::display::ReadPrimaryDisplayGeometry();
  if (display.ok() && display->width_pixels > 0 && display->height_pixels > 0) {
    snapshot.display = symbian::agent::GuestDisplaySnapshot{
        static_cast<std::uint32_t>(display->width_pixels),
        static_cast<std::uint32_t>(display->height_pixels)};
  }
  return snapshot;
}

void Serve(TcpClient client, symbian::agent::AgentLogRing& log) {
  auto server =
      TlsServer::Create(k_server_cert, k_server_key, k_server_cert,
                        symbian::api::connectivity::TlsVersion::kTls13);
  if (!server.ok() || !server->Accept(std::move(client), 10s).ok()) {
    return;
  }
  log.Append(symbian::agent::AgentLogCode::kAuthenticated);

  struct CloseLog {
    symbian::agent::AgentLogRing& log;

    ~CloseLog() { log.Append(symbian::agent::AgentLogCode::kSessionClosed); }
  } close_log{log};

  bool negotiated = false;
  for (std::size_t request_index = 0;
       request_index < kMaximumRequestsPerConnection; ++request_index) {
    const auto deadline = std::chrono::steady_clock::now() + kControlDeadline;
    std::array<std::uint8_t, 4> prefix{};
    if (!ReadExactly(*server, prefix, deadline)) {
      return;
    }
    const std::uint32_t length = (static_cast<std::uint32_t>(prefix[0]) << 24) |
                                 (static_cast<std::uint32_t>(prefix[1]) << 16) |
                                 (static_cast<std::uint32_t>(prefix[2]) << 8) |
                                 static_cast<std::uint32_t>(prefix[3]);
    if (length == 0 || length > kMaximumFrame) {
      log.Append(symbian::agent::AgentLogCode::kRejectedFrame);
      return;
    }
    std::array<std::uint8_t, kMaximumFrame> payload{};
    if (!ReadExactly(*server, std::span(payload).first(length), deadline)) {
      return;
    }
    auto request = symbian::agent::ParseGuestControl(std::string_view(
        reinterpret_cast<const char*>(payload.data()), length));
    if (!request.ok()) {
      log.Append(symbian::agent::AgentLogCode::kRejectedFrame);
      return;
    }
    if ((!negotiated && request->kind != 1) ||
        (negotiated && request->kind == 1)) {
      log.Append(symbian::agent::AgentLogCode::kRejectedFrame);
      return;
    }
    absl::StatusOr<std::string> response =
        absl::InvalidArgumentError("Invalid agent operation");
    if (request->kind == 1) {
      response = symbian::agent::PackGuestHelloResult(
          *request, true, kMaximumRequestsPerConnection);
    } else if (request->kind == 6) {
      auto page = log.ReadAfter(request->log_after, request->log_limit);
      if (!page.ok()) {
        return;
      }
      response = symbian::agent::PackGuestLogResult(*request, *page);
    } else {
      response =
          symbian::agent::PackGuestResult(*request, ReadStatusSnapshot());
    }
    if (!response.ok()) {
      return;
    }
    const auto count = static_cast<std::uint32_t>(response->size());
    const std::array<std::uint8_t, 4> response_prefix{
        static_cast<std::uint8_t>(count >> 24),
        static_cast<std::uint8_t>(count >> 16),
        static_cast<std::uint8_t>(count >> 8),
        static_cast<std::uint8_t>(count)};
    if (!WriteExactly(*server, response_prefix, deadline) ||
        !WriteExactly(
            *server,
            std::span(reinterpret_cast<const std::uint8_t*>(response->data()),
                      response->size()),
            deadline)) {
      return;
    }
    if (request->kind == 2) {
      log.Append(symbian::agent::AgentLogCode::kStatusRead);
    } else if (request->kind == 1) {
      negotiated = true;
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
          [client = std::move(*result), log = log_]() mutable {
            Serve(std::move(client), *log);
          },
          kTlsWorkerStackBytes);
    }
    listener_.AcceptNext();
  }

  void StopFromLocalUi() {
    listener_.Stop();
    ProbeStopScheduler();
  }

 private:
  std::shared_ptr<symbian::agent::AgentLogRing> log_ =
      std::make_shared<symbian::agent::AgentLogRing>();
  symbian::concurrency::WorkerExecutor worker_{4};
  symbian::api::connectivity::ActiveTcpListener listener_;
};

AgentService* current_service = nullptr;
std::atomic<bool>* current_stop_request = nullptr;

}  // namespace

extern "C" bool AgentLocalStopRequested() {
  return current_stop_request != nullptr && current_stop_request->load();
}

extern "C" void AgentStopOnScheduler() {
  if (current_service != nullptr) {
    current_service->StopFromLocalUi();
  }
}

extern "C" int RunActiveProbe() {
  AgentService service;
  absl::Status start = service.Start();
  if (!start.ok()) {
    // A second menu launch signals the resident instance. Other failures
    // remain startup failures rather than silently claiming a live agent.
    if (symbian::NativeErrorFromStatus(start) == -14 &&
        AgentRequestForeground() == 0) {
      return 0;
    }
    return -301;
  }
  std::atomic<bool> stop_requested{false};
  std::thread ui_thread([&] { AgentLocalUiMain(&stop_requested); });
  current_service = &service;
  current_stop_request = &stop_requested;
  ProbeStartScheduler();
  stop_requested.store(true);
  current_stop_request = nullptr;
  current_service = nullptr;
  ui_thread.join();
  return 0;
}
