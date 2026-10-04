// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.
// Manually started, loopback-only development agent. The bundled certificate
// is a public test fixture and has no physical-device security value.

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "agent_certificates.h"
#include "agent_signals.h"
#include "symbian/agent/guest_control.h"
#include "symbian/agent/guest_log.h"
#include "symbian/api/connectivity/active_tcp_listener.h"
#include "symbian/api/connectivity/tls_server.h"
#include "symbian/api/display/display.h"
#include "symbian/api/display/resident_panel.h"
#include "symbian/api/system/active_service.h"
#include "symbian/api/system/counters.h"
#include "symbian/concurrency/worker_executor.h"
#include "symbian/native_status.h"

namespace {

using symbian::api::connectivity::TcpClient;
using symbian::api::connectivity::TlsServer;
constexpr std::uint16_t kAgentPort = 39101;
constexpr std::size_t kMaximumFrame = 4096;
constexpr std::size_t kMaximumRequestsPerConnection = 16;
constexpr std::size_t kTlsWorkerStackBytes = 256 * 1024;
const absl::Duration kControlDeadline = absl::Seconds(5);

absl::Duration Remaining(absl::Time deadline) {
  const absl::Time now = absl::Now();
  if (now >= deadline) {
    return absl::ZeroDuration();
  }
  return std::max(absl::Milliseconds(1), std::move(deadline) - now);
}

bool ReadExactly(TlsServer& server, std::span<std::uint8_t> output,
                 absl::Time deadline) {
  while (!output.empty()) {
    const auto remaining = Remaining(deadline);
    if (remaining == absl::ZeroDuration()) {
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
                  absl::Time deadline) {
  // TlsServer::WriteFor writes the entire input or returns an error.
  const auto remaining = Remaining(std::move(deadline));
  return remaining != absl::ZeroDuration() &&
         server.WriteFor(input, remaining).ok();
}

symbian::agent::GuestStatusSnapshot ReadStatusSnapshot() {
  symbian::agent::GuestStatusSnapshot snapshot;
  snapshot.logs_available = true;
  auto tick = symbian::api::system::ReadTickCounter();
  if (tick.ok() && tick->period > absl::ZeroDuration()) {
    snapshot.tick = symbian::agent::GuestTickSnapshot{
        tick->count,
        static_cast<std::uint64_t>(absl::ToInt64Microseconds(tick->period))};
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
  if (!server.ok() ||
      !server->Accept(std::move(client), absl::Seconds(10)).ok()) {
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
    const absl::Time deadline = absl::Now() + kControlDeadline;
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
    if (stopping_) {
      return;
    }
    if (result.ok()) {
      // The bounded queue closes a rejected client through its captured owner.
      worker_.PostFiber(
          [client = std::move(*result), log = log_]() mutable {
            Serve(std::move(client), *log);
          },
          kTlsWorkerStackBytes);
    }
    if (!listener_.AcceptNext().ok()) {
      StopFromEventCallback();
    }
  }

  void CloseListener() {
    if (stopping_) {
      return;
    }
    stopping_ = true;
    listener_.Stop();
  }

  void StopFromEventCallback() {
    CloseListener();
    symbian::api::system::StopActiveService();
  }

 private:
  std::shared_ptr<symbian::agent::AgentLogRing> log_ =
      std::make_shared<symbian::agent::AgentLogRing>();
  symbian::concurrency::WorkerExecutor worker_{4};
  symbian::api::connectivity::ActiveTcpListener listener_;
  bool stopping_ = false;
};

constexpr symbian::api::display::ResidentPanelOptions kPanel{
    .app_uid = 0xe0000a31u,
    .property_category = agent_service::kPropertyCategory,
    .foreground_key = agent_service::kRaisePanelKey,
    .caption = "Development Agent",
    .heading = "AGENT",
    .state = "RUNNING",
    .back_label = "BACK",
    .stop_label = "STOP",
};

}  // namespace

absl::Status RunAgentService() {
  AgentService service;
  std::atomic<bool> stop_requested{false};
  std::thread ui_thread;
  absl::Status panel_status = absl::OkStatus();
  const absl::Status result = symbian::api::system::RunActiveService(
      agent_service::kPropertyCategory, agent_service::kStopServiceKey,
      [&] { return service.Start(); }, [&] { service.CloseListener(); },
      [&] {
        ui_thread = std::thread([&] {
          panel_status =
              symbian::api::display::RunResidentPanel(kPanel, stop_requested);
          if (!panel_status.ok()) {
            stop_requested.store(true);
          }
          if (stop_requested.load()) {
            // The scheduler may already have stopped before this thread exits.
            symbian::api::system::RequestActiveServiceStop(
                agent_service::kPropertyCategory,
                agent_service::kStopServiceKey)
                .IgnoreError();
          }
        });
      });
  stop_requested.store(true);
  if (ui_thread.joinable()) {
    ui_thread.join();
  }
  if (!result.ok()) {
    // A second menu launch raises the panel of the resident instance.
    if (symbian::NativeErrorFromStatus(result) ==
        symbian::native_error::kInUse) {
      return symbian::api::display::RequestResidentPanelForeground(
          agent_service::kPropertyCategory, agent_service::kRaisePanelKey);
    }
    return result;
  }
  return panel_status;
}

extern "C" int RuntimeMain() {
  return symbian::NativeErrorFromStatus(RunAgentService());
}
