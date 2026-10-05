// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.
// Manually started development agent. Public-key emulator builds bind loopback.

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
#include "agent_key.h"
#include "agent_signals.h"
#include "mbedtls/entropy.h"
#include "mbedtls/md.h"
#include "mbedtls/platform_util.h"
#include "symbian/agent/guest_control.h"
#include "symbian/agent/guest_files.h"
#include "symbian/agent/guest_log.h"
#include "symbian/api/connectivity/active_tcp_listener.h"
#include "symbian/api/connectivity/broadcast_probe.h"
#include "symbian/api/connectivity/tcp_client.h"
#include "symbian/api/connectivity/websocket.h"
#include "symbian/api/display/display.h"
#include "symbian/api/display/resident_panel.h"
#include "symbian/api/system/active_service.h"
#include "symbian/api/system/counters.h"
#include "symbian/concurrency/worker_executor.h"
#include "symbian/native_status.h"

namespace {

using symbian::api::connectivity::TcpClient;
using symbian::api::connectivity::WebSocketStream;
constexpr std::uint16_t kAgentPort = 39101;
constexpr std::uint16_t kDiscoveryPort = 39104;
constexpr std::uint16_t kHostPort = 39103;
constexpr std::size_t kMaximumFrame = 4096;
constexpr std::size_t kMaximumRequestsPerConnection = 16;
constexpr std::size_t kWorkerStackBytes = 256 * 1024;
const absl::Duration kControlDeadline = absl::Seconds(5);

enum class LinkPhase : std::uint8_t {
  kSearching,
  kNoOffer,
  kProbeError,
  kDialing,
  kDialError,
  kAuthenticating,
};

std::atomic<LinkPhase> link_phase{LinkPhase::kSearching};

const char* AgentHeading() {
  switch (link_phase.load()) {
    case LinkPhase::kSearching:
      return "SEARCH";
    case LinkPhase::kNoOffer:
      return "NO OFFER";
    case LinkPhase::kProbeError:
      return "PROBE ERR";
    case LinkPhase::kDialing:
      return "DIALING";
    case LinkPhase::kDialError:
      return "DIAL ERR";
    case LinkPhase::kAuthenticating:
      return "AUTH";
  }
  return "AGENT";
}

absl::Duration Remaining(absl::Time deadline) {
  const absl::Time now = absl::Now();
  if (now >= deadline) {
    return absl::ZeroDuration();
  }
  return std::max(absl::Milliseconds(1), std::move(deadline) - now);
}

bool ReadExactly(WebSocketStream& client, std::span<std::uint8_t> output,
                 absl::Time deadline) {
  while (!output.empty()) {
    const auto remaining = Remaining(deadline);
    if (remaining == absl::ZeroDuration()) {
      return false;
    }
    auto received = client.Receive(output, deadline);
    if (!received.ok() || *received == 0) {
      return false;
    }
    output = output.subspan(*received);
  }
  return true;
}

bool WriteExactly(WebSocketStream& client, std::span<const std::uint8_t> input,
                  absl::Time deadline) {
  const auto remaining = Remaining(std::move(deadline));
  return remaining != absl::ZeroDuration() && client.Send(input, deadline).ok();
}

std::array<std::uint8_t, 32> AgentKey() {
  constexpr char hex[] = SYMBIAN_AGENT_KEY_HEX;
  auto nibble = [](char digit) -> std::uint8_t {
    return digit >= 'a' ? static_cast<std::uint8_t>(digit - 'a' + 10)
                        : static_cast<std::uint8_t>(digit - '0');
  };
  std::array<std::uint8_t, 32> key{};
  for (std::size_t index = 0; index < key.size(); ++index) {
    key[index] = static_cast<std::uint8_t>((nibble(hex[2 * index]) << 4) |
                                           nibble(hex[2 * index + 1]));
  }
  return key;
}

absl::StatusOr<std::array<std::uint8_t, 4>> DiscoverHost() {
  std::array<std::uint8_t, 8> nonce{};
  mbedtls_entropy_context entropy;
  mbedtls_entropy_init(&entropy);
  const int random_result =
      mbedtls_entropy_func(&entropy, nonce.data(), nonce.size());
  mbedtls_entropy_free(&entropy);
  if (random_result != 0) {
    return absl::UnavailableError("Agent discovery entropy unavailable");
  }
  const auto key = AgentKey();
  const mbedtls_md_info_t* md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (md == nullptr) {
    return absl::InternalError("Agent discovery HMAC unavailable");
  }
  auto message = [&](std::string_view label,
                     std::array<std::uint8_t, 32>& mac) {
    std::array<std::uint8_t, 64> input{};
    std::copy(label.begin(), label.end(), input.begin());
    std::copy(nonce.begin(), nonce.end(), input.begin() + label.size());
    return mbedtls_md_hmac(md, key.data(), key.size(), input.data(),
                           label.size() + nonce.size(), mac.data()) == 0;
  };
  std::array<std::uint8_t, 45> request{};
  std::array<std::uint8_t, 45> response{};
  std::copy_n("SAGD1", 5, request.begin());
  std::copy_n("SAGR1", 5, response.begin());
  std::copy(nonce.begin(), nonce.end(), request.begin() + 5);
  std::copy(nonce.begin(), nonce.end(), response.begin() + 5);
  std::array<std::uint8_t, 32> request_mac{};
  std::array<std::uint8_t, 32> response_mac{};
  if (!message("symbian-agent-discover-v1", request_mac) ||
      !message("symbian-agent-offer-v1", response_mac)) {
    return absl::InternalError("Agent discovery HMAC failed");
  }
  std::copy(request_mac.begin(), request_mac.end(), request.begin() + 13);
  std::copy(response_mac.begin(), response_mac.end(), response.begin() + 13);
  return symbian::api::connectivity::BroadcastProbe(
      kDiscoveryPort, request, response, absl::Now() + absl::Seconds(3));
}

bool Authenticate(WebSocketStream& client) {
  constexpr std::string_view kClientLabel = "symbian-agent-client-v1";
  constexpr std::string_view kServerLabel = "symbian-agent-server-v1";
  std::array<std::uint8_t, 32> nonce{};
  mbedtls_entropy_context entropy;
  mbedtls_entropy_init(&entropy);
  const int random_result =
      mbedtls_entropy_func(&entropy, nonce.data(), nonce.size());
  mbedtls_entropy_free(&entropy);
  if (random_result != 0) {
    return false;
  }
  const absl::Time deadline = absl::Now() + kControlDeadline;
  std::array<std::uint8_t, 36> challenge{};
  challenge[0] = 'S';
  challenge[1] = 'A';
  challenge[2] = 'G';
  challenge[3] = '1';
  std::copy(nonce.begin(), nonce.end(), challenge.begin() + 4);
  if (!WriteExactly(client, challenge, deadline)) {
    return false;
  }
  std::array<std::uint8_t, 64> reply{};
  if (!ReadExactly(client, reply, deadline)) {
    return false;
  }
  const mbedtls_md_info_t* md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (md == nullptr) {
    return false;
  }
  const auto key = AgentKey();
  auto digest = [&](std::string_view label,
                    std::array<std::uint8_t, 32>& result) {
    std::array<std::uint8_t, 96> message{};
    std::copy(label.begin(), label.end(), message.begin());
    std::copy(nonce.begin(), nonce.end(), message.begin() + label.size());
    std::copy_n(reply.begin(), 32, message.begin() + label.size() + 32);
    return mbedtls_md_hmac(md, key.data(), key.size(), message.data(),
                           label.size() + 64, result.data()) == 0;
  };
  std::array<std::uint8_t, 32> expected{};
  if (!digest(kClientLabel, expected)) {
    return false;
  }
  std::uint8_t difference = 0;
  for (std::size_t index = 0; index < expected.size(); ++index) {
    difference |= expected[index] ^ reply[32 + index];
  }
  if (difference != 0) {
    return false;
  }
  std::array<std::uint8_t, 32> proof{};
  const bool authenticated =
      digest(kServerLabel, proof) && WriteExactly(client, proof, deadline);
  mbedtls_platform_zeroize(reply.data(), reply.size());
  return authenticated;
}

symbian::agent::GuestStatusSnapshot ReadStatusSnapshot() {
  symbian::agent::GuestStatusSnapshot snapshot;
  snapshot.logs_available = true;
  snapshot.workspace_available = true;
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

void Serve(TcpClient raw, symbian::agent::AgentLogRing& log) {
  symbian::websocket::Options options;
  options.mask_provider = []() -> absl::StatusOr<std::array<std::uint8_t, 4>> {
    std::array<std::uint8_t, 4> mask{};
    mbedtls_entropy_context entropy;
    mbedtls_entropy_init(&entropy);
    const int result = mbedtls_entropy_func(&entropy, mask.data(), mask.size());
    mbedtls_entropy_free(&entropy);
    if (result != 0) {
      return absl::UnavailableError("WebSocket masking entropy unavailable");
    }
    return mask;
  };
  const auto deadline = absl::Now() + kControlDeadline;
#if SYMBIAN_AGENT_PRIVATE_PROFILE
  auto opened =
      WebSocketStream::Connect(std::move(raw), std::move(options), deadline);
#else
  auto opened =
      WebSocketStream::Accept(std::move(raw), std::move(options), deadline);
#endif
  if (!opened.ok()) {
    return;
  }
  auto client = std::move(*opened);
  if (!Authenticate(client)) {
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
    if (!ReadExactly(client, prefix, deadline)) {
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
    if (!ReadExactly(client, std::span(payload).first(length), deadline)) {
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
          *request, true, kMaximumRequestsPerConnection, true);
    } else if (request->kind == 6) {
      auto page = log.ReadAfter(request->page_after, request->page_limit);
      if (!page.ok()) {
        return;
      }
      response = symbian::agent::PackGuestLogResult(*request, *page);
    } else if (request->kind == 7) {
      if (request->page_after >= 256) {
        return;
      }
      auto page = symbian::agent::ReadWorkspacePage(
          static_cast<std::uint16_t>(request->page_after), request->page_limit);
      if (!page.ok()) {
        return;
      }
      response = symbian::agent::PackGuestWorkspaceResult(*request, *page);
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
    std::string framed(reinterpret_cast<const char*>(response_prefix.data()),
                       response_prefix.size());
    framed.append(*response);
    if (!WriteExactly(
            client,
            std::span(reinterpret_cast<const std::uint8_t*>(framed.data()),
                      framed.size()),
            deadline)) {
      return;
    }
    if (request->kind == 2) {
      log.Append(symbian::agent::AgentLogCode::kStatusRead);
    } else if (request->kind == 1) {
      negotiated = true;
    }
  }
  client.Close(absl::Now() + kControlDeadline).IgnoreError();
}

class AgentService final
    : public symbian::api::connectivity::TcpAcceptObserver {
 public:
  AgentService() : listener_(*this) {}

  absl::Status Start() {
    if (SYMBIAN_AGENT_PRIVATE_PROFILE) {
      auto task = worker_.PostFiber(
          [stop = stopping_, log = log_] {
            while (!stop->load()) {
              auto host = DiscoverHost();
              if (host.ok() && !stop->load()) {
                link_phase.store(LinkPhase::kDialing);
                auto client = TcpClient::ConnectIpv4(
                    *host, kHostPort, absl::Now() + absl::Seconds(3));
                if (client.ok() && !stop->load()) {
                  link_phase.store(LinkPhase::kAuthenticating);
                  Serve(std::move(*client), *log);
                } else if (!stop->load()) {
                  link_phase.store(LinkPhase::kDialError);
                }
              } else if (!stop->load()) {
                link_phase.store(host.status().code() ==
                                         absl::StatusCode::kDeadlineExceeded
                                     ? LinkPhase::kNoOffer
                                     : LinkPhase::kProbeError);
              }
              for (int tick = 0; tick != 8 && !stop->load(); ++tick) {
                absl::SleepFor(absl::Milliseconds(250));
              }
            }
          },
          kWorkerStackBytes);
      auto result = task.ResultIfReady();
      return result ? result->status() : absl::OkStatus();
    }
    absl::Status status = listener_.EnableWorkerSharing();
    if (!status.ok()) {
      return status;
    }
    return listener_.ListenIpv4({127, 0, 0, 1}, kAgentPort);
  }

  void OnAccept(absl::StatusOr<TcpClient> result) override {
    if (stopping_local_) {
      return;
    }
    if (result.ok()) {
      // The bounded queue closes a rejected client through its captured owner.
      worker_.PostFiber(
          [client = std::move(*result), log = log_]() mutable {
            Serve(std::move(client), *log);
          },
          kWorkerStackBytes);
    }
    if (!listener_.AcceptNext().ok()) {
      StopFromEventCallback();
    }
  }

  void CloseListener() {
    if (stopping_local_) {
      return;
    }
    stopping_local_ = true;
    stopping_->store(true);
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
  std::shared_ptr<std::atomic<bool>> stopping_ =
      std::make_shared<std::atomic<bool>>(false);
  bool stopping_local_ = false;
};

constexpr symbian::api::display::ResidentPanelOptions kPanel{
    .app_uid = 0xe0000a31u,
    .property_category = agent_service::kPropertyCategory,
    .foreground_key = agent_service::kRaisePanelKey,
    .caption = "Development Agent",
    .heading = "AGENT",
    .state = SYMBIAN_AGENT_PANEL_STATE,
    .back_label = "BACK",
    .stop_label = "STOP",
    .heading_provider = SYMBIAN_AGENT_PRIVATE_PROFILE ? AgentHeading : nullptr,
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
