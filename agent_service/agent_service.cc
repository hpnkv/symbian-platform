// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.
// Manually started development agent. Public-key emulator builds bind loopback.

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <absl/base/nullability.h>
#include <absl/status/status_macros.h>

#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "agent_identity.h"
#include "agent_key.h"
#include "agent_signals.h"
#include "mbedtls/entropy.h"
#include "mbedtls/md.h"
#include "mbedtls/platform_util.h"
#include "resident_panel.h"
#include "symbian/agent/guest_control.h"
#include "symbian/agent/guest_files.h"
#include "symbian/agent/guest_log.h"
#include "symbian/api/connectivity/active_tcp_listener.h"
#include "symbian/api/connectivity/broadcast_probe.h"
#include "symbian/api/connectivity/tcp_client.h"
#include "symbian/api/connectivity/websocket.h"
#include "symbian/api/display/display.h"
#include "symbian/api/display/screen_control.h"
#include "symbian/api/storage/storage.h"
#include "symbian/api/system/active_service.h"
#include "symbian/api/system/application_management.h"
#include "symbian/api/system/counters.h"
#include "symbian/api/system/debug_log.h"
#include "symbian/api/system/failure_handler.h"
#include "symbian/api/system/serial_ports.h"
#include "symbian/concurrency/mutex.h"
#include "symbian/concurrency/worker_executor.h"
#include "symbian/native_status.h"

namespace {

using symbian::api::connectivity::TcpClient;
using symbian::api::connectivity::WebSocketStream;
constexpr std::uint16_t kAgentPort = 39101;
constexpr std::uint16_t kDiscoveryPort = 39104;
constexpr std::uint16_t kHostPort = 39103;
constexpr std::size_t kMaximumFrame = 4096;
constexpr std::size_t kMaximumRequestsPerConnection = 1024;
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

void TraceStartup(std::string_view message) {
  static thread::Mutex mutex;
  thread::MutexLock lock(&mutex);
  static std::size_t offset = 0;
  constexpr std::u16string_view kPath = u"E:\\Others\\agent_startup.txt";
  if (offset == 0) {
    symbian::api::storage::CreateDirectories(u"E:\\Others").IgnoreError();
  }
  auto file = symbian::api::storage::WritableFile::Open(
      kPath, offset == 0 ? symbian::api::storage::WriteMode::kReplaceExisting
                         : symbian::api::storage::WriteMode::kOpenExisting);
  if (!file.ok()) {
    return;
  }
  const auto* absl_nonnull bytes =
      reinterpret_cast<const std::byte*>(message.data());
  if (file->WriteAt(offset, std::span(bytes, message.size())).ok() &&
      file->WriteAt(offset + message.size(),
                    std::span(reinterpret_cast<const std::byte*>("\n"), 1))
          .ok()) {
    offset += message.size() + 1;
    file->Flush().IgnoreError();
  }
}

void TraceSerialPorts() {
  auto ports = symbian::api::system::ListSerialPortRanges();
  if (!ports.ok()) {
    TraceStartup("C32 inventory: " + ports.status().ToString());
    return;
  }
  TraceStartup("C32 port ranges: " + std::to_string(ports->size()));
  for (const auto& port : *ports) {
    std::string label = "C32 port ";
    for (char16_t character : port.name) {
      label.push_back(character >= 32 && character < 127
                          ? static_cast<char>(character)
                          : '?');
    }
    label += " units " + std::to_string(port.first_unit) + ".." +
             std::to_string(port.last_unit);
    TraceStartup(label);
  }
}

const char* absl_nullable AgentHeading() {
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

bool ReadExactly(WebSocketStream* absl_nonnull client,
                 std::span<std::uint8_t> output, absl::Time deadline) {
  while (!output.empty()) {
    if (const auto remaining = Remaining(deadline);
        remaining == absl::ZeroDuration()) {
      return false;
    }
    auto received = client->Receive(output, deadline);
    if (!received.ok() || *received == 0) {
      return false;
    }
    output = output.subspan(*received);
  }
  return true;
}

bool WriteExactly(WebSocketStream* absl_nonnull client,
                  std::span<const std::uint8_t> input, absl::Time deadline) {
  const auto remaining = Remaining(std::move(deadline));
  return remaining != absl::ZeroDuration() &&
         client->Send(input, deadline).ok();
}

absl::StatusOr<std::array<std::uint8_t, 4>> DiscoverHost(
    const agent_service::AgentIdentity::Key& key) {
  std::array<std::uint8_t, 8> nonce{};
  mbedtls_entropy_context entropy;
  mbedtls_entropy_init(&entropy);
  const int random_result =
      mbedtls_entropy_func(&entropy, nonce.data(), nonce.size());
  mbedtls_entropy_free(&entropy);
  if (random_result != 0) {
    return absl::UnavailableError("Agent discovery entropy unavailable");
  }
  const mbedtls_md_info_t* absl_nullable md =
      mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (md == nullptr) {
    return absl::InternalError("Agent discovery HMAC unavailable");
  }
  auto message = [&](std::string_view label,
                     std::array<std::uint8_t, 32>* absl_nonnull mac) {
    std::array<std::uint8_t, 64> input{};
    std::copy(label.begin(), label.end(), input.begin());
    std::copy(nonce.begin(), nonce.end(), input.begin() + label.size());
    return mbedtls_md_hmac(md, key.data(), key.size(), input.data(),
                           label.size() + nonce.size(), mac->data()) == 0;
  };
  std::array<std::uint8_t, 45> request{};
  std::array<std::uint8_t, 45> response{};
  std::copy_n("SAGD1", 5, request.begin());
  std::copy_n("SAGR1", 5, response.begin());
  std::copy(nonce.begin(), nonce.end(), request.begin() + 5);
  std::copy(nonce.begin(), nonce.end(), response.begin() + 5);
  std::array<std::uint8_t, 32> request_mac{};
  std::array<std::uint8_t, 32> response_mac{};
  if (!message("symbian-agent-discover-v1", &request_mac) ||
      !message("symbian-agent-offer-v1", &response_mac)) {
    return absl::InternalError("Agent discovery HMAC failed");
  }
  std::copy(request_mac.begin(), request_mac.end(), request.begin() + 13);
  std::copy(response_mac.begin(), response_mac.end(), response.begin() + 13);
  return symbian::api::connectivity::BroadcastProbe(
      kDiscoveryPort, request, response, absl::Now() + absl::Seconds(3));
}

bool Authenticate(WebSocketStream* absl_nonnull client,
                  const agent_service::AgentIdentity::Key& key) {
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
  const mbedtls_md_info_t* absl_nullable md =
      mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (md == nullptr) {
    return false;
  }
  auto digest = [&](std::string_view label,
                    std::array<std::uint8_t, 32>* absl_nonnull result) {
    std::array<std::uint8_t, 96> message{};
    std::copy(label.begin(), label.end(), message.begin());
    std::copy(nonce.begin(), nonce.end(), message.begin() + label.size());
    std::copy_n(reply.begin(), 32, message.begin() + label.size() + 32);
    return mbedtls_md_hmac(md, key.data(), key.size(), message.data(),
                           label.size() + 64, result->data()) == 0;
  };
  std::array<std::uint8_t, 32> expected{};
  if (!digest(kClientLabel, &expected)) {
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
      digest(kServerLabel, &proof) && WriteExactly(client, proof, deadline);
  mbedtls_platform_zeroize(reply.data(), reply.size());
  return authenticated;
}

symbian::agent::GuestStatusSnapshot ReadStatusSnapshot() {
  symbian::agent::GuestStatusSnapshot snapshot;
  snapshot.logs_available = true;
  snapshot.workspace_available = true;
  if (auto tick = symbian::api::system::ReadTickCounter();
      tick.ok() && tick->period > absl::ZeroDuration()) {
    snapshot.tick = symbian::agent::GuestTickSnapshot{
        .count = tick->count,
        .period_microseconds = static_cast<std::uint64_t>(
            absl::ToInt64Microseconds(tick->period))};
  }
  if (auto display = symbian::api::display::ReadPrimaryDisplayGeometry();
      display.ok() && display->width_pixels > 0 && display->height_pixels > 0) {
    snapshot.display = symbian::agent::GuestDisplaySnapshot{
        .width_pixels = static_cast<std::uint32_t>(display->width_pixels),
        .height_pixels = static_cast<std::uint32_t>(display->height_pixels)};
  }
  return snapshot;
}

struct AgentResponse {
  std::string control;
  std::vector<std::uint8_t> data;
};

absl::StatusOr<AgentResponse> Dispatch(
    const symbian::agent::GuestControlRequest* absl_nonnull request,
    std::span<const std::uint8_t> input_data,
    symbian::agent::AgentLogRing* absl_nonnull log) {
  absl::StatusOr<std::string> response =
      absl::InvalidArgumentError("Invalid agent operation");
  std::vector<std::uint8_t> output_data;
  if (request->kind == 1) {
    response = symbian::agent::PackGuestHelloResult(
        *request, true, kMaximumRequestsPerConnection, true);
  } else if (request->kind == 6) {
    if (auto page = log->ReadAfter(request->page_after, request->page_limit);
        !page.ok()) {
      response = symbian::agent::PackGuestError(*request, page.status());
    } else {
      response = symbian::agent::PackGuestLogResult(*request, *page);
    }
  } else if (request->kind == 7) {
    if (request->page_after >= 256) {
      response = symbian::agent::PackGuestError(
          *request, absl::OutOfRangeError("Workspace cursor exceeds 256"));
    } else {
      auto page = symbian::agent::ReadWorkspacePage(
          static_cast<std::uint16_t>(request->page_after), request->page_limit);
      response = page.ok()
                     ? symbian::agent::PackGuestWorkspaceResult(*request, *page)
                     : symbian::agent::PackGuestError(*request, page.status());
    }
  } else if (request->kind == 8) {
    if (auto capture = symbian::api::display::CapturePrimaryScreen();
        !capture.ok()) {
      response = symbian::agent::PackGuestError(*request, capture.status());
    } else {
      response = symbian::agent::PackGuestScreenResult(
          *request, capture->width, capture->height, capture->stride_bytes,
          capture->pixels.size());
      output_data = std::move(capture->pixels);
    }
  } else if (request->kind == 9) {
    using symbian::api::display::PointerAction;
    const PointerAction action =
        request->pointer_action == 1   ? PointerAction::kMove
        : request->pointer_action == 2 ? PointerAction::kDown
                                       : PointerAction::kUp;
    absl::Status submitted = symbian::api::display::SendPrimaryPointerEvent(
        action, request->pointer_x, request->pointer_y);
    response = submitted.ok()
                   ? symbian::agent::PackGuestPointerResult(*request)
                   : symbian::agent::PackGuestError(*request, submitted);
  } else if (request->kind == 10) {
    if (auto chunk = symbian::agent::ReadResourceChunk(
            request->resource_scope, request->resource_uid,
            request->resource_name, request->resource_offset,
            request->resource_length);
        !chunk.ok()) {
      response = symbian::agent::PackGuestError(*request, chunk.status());
    } else {
      response = symbian::agent::PackGuestResourceReadResult(
          *request, chunk->total_bytes, chunk->bytes.size());
      output_data = std::move(chunk->bytes);
    }
  } else if (request->kind == 11) {
    if (input_data.size() != request->resource_length) {
      return absl::InvalidArgumentError("Agent write payload length differs");
    }
    if (absl::Status written = symbian::agent::WriteResourceChunk(
            request->resource_scope, request->resource_uid,
            request->resource_name, request->resource_offset,
            request->resource_mode,
            std::span(reinterpret_cast<const std::byte*>(input_data.data()),
                      input_data.size()));
        !written.ok()) {
      response = symbian::agent::PackGuestError(*request, written);
    } else {
      response = symbian::agent::PackGuestResourceWriteResult(*request);
    }
  } else if (request->kind == 12) {
    if (auto registered = symbian::api::system::IsApplicationRegistered(
            request->resource_uid);
        !registered.ok()) {
      response = symbian::agent::PackGuestError(*request, registered.status());
    } else {
      std::u16string path = u"C:\\private\\e0000a31\\workspace\\";
      for (char ch : request->resource_name) {
        path.push_back(static_cast<char16_t>(ch));
      }
      absl::Status launched = symbian::api::system::OpenDocument(path);
      response = launched.ok()
                     ? symbian::agent::PackGuestPackageOpenResult(*request,
                                                                  *registered)
                     : symbian::agent::PackGuestError(*request, launched);
    }
  } else if (request->kind == 13) {
    if (auto registered = symbian::api::system::IsApplicationRegistered(
            request->resource_uid);
        !registered.ok()) {
      response = symbian::agent::PackGuestError(*request, registered.status());
    } else {
      response =
          symbian::agent::PackGuestAppRegisteredResult(*request, *registered);
    }
  } else {
    response = symbian::agent::PackGuestResult(*request, ReadStatusSnapshot());
  }
  ABSL_RETURN_IF_ERROR(response.status());
  return AgentResponse{.control = std::move(*response),
                       .data = std::move(output_data)};
}

void Serve(TcpClient raw,
           std::shared_ptr<const agent_service::AgentIdentity> identity,
           symbian::agent::AgentLogRing* absl_nonnull log) {
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
  if (!Authenticate(&client, identity->key())) {
    return;
  }
  log->Append(symbian::agent::AgentLogCode::kAuthenticated);

  struct CloseLog {
    symbian::agent::AgentLogRing& log;

    ~CloseLog() { log.Append(symbian::agent::AgentLogCode::kSessionClosed); }
  } close_log{*log};

  bool negotiated = false;
  for (std::size_t request_index = 0;
       request_index < kMaximumRequestsPerConnection; ++request_index) {
    const absl::Time deadline = absl::Now() + kControlDeadline;
    std::array<std::uint8_t, 4> prefix{};
    if (!ReadExactly(&client, prefix, deadline)) {
      return;
    }
    const std::uint32_t length = (static_cast<std::uint32_t>(prefix[0]) << 24) |
                                 (static_cast<std::uint32_t>(prefix[1]) << 16) |
                                 (static_cast<std::uint32_t>(prefix[2]) << 8) |
                                 static_cast<std::uint32_t>(prefix[3]);
    if (length == 0 || length > kMaximumFrame) {
      log->Append(symbian::agent::AgentLogCode::kRejectedFrame);
      return;
    }
    std::array<std::uint8_t, kMaximumFrame> payload{};
    if (!ReadExactly(&client, std::span(payload).first(length), deadline)) {
      return;
    }
    auto request = symbian::agent::ParseGuestControl(std::string_view(
        reinterpret_cast<const char*>(payload.data()), length));
    if (!request.ok()) {
      log->Append(symbian::agent::AgentLogCode::kRejectedFrame);
      return;
    }
    if ((!negotiated && request->kind != 1) ||
        (negotiated && request->kind == 1)) {
      log->Append(symbian::agent::AgentLogCode::kRejectedFrame);
      return;
    }
    std::vector<std::uint8_t> input_data;
    if (request->kind == 11) {
      input_data.resize(request->resource_length);
      if (!ReadExactly(&client, std::span(input_data),
                       absl::Now() + absl::Seconds(15))) {
        return;
      }
    }
    auto dispatched = Dispatch(&*request, input_data, log);
    if (!dispatched.ok()) {
      return;
    }
    auto& response = dispatched->control;
    auto& output_data = dispatched->data;
    const auto count = static_cast<std::uint32_t>(response.size());
    const std::array<std::uint8_t, 4> response_prefix{
        static_cast<std::uint8_t>(count >> 24),
        static_cast<std::uint8_t>(count >> 16),
        static_cast<std::uint8_t>(count >> 8),
        static_cast<std::uint8_t>(count)};
    std::string framed(reinterpret_cast<const char*>(response_prefix.data()),
                       response_prefix.size());
    framed.append(response);
    if (!WriteExactly(
            &client,
            std::span(reinterpret_cast<const std::uint8_t*>(framed.data()),
                      framed.size()),
            deadline)) {
      return;
    }
    const absl::Time screen_deadline = absl::Now() + absl::Seconds(15);
    for (std::size_t offset = 0; offset < output_data.size(); offset += 4096) {
      if (const std::size_t count =
              std::min<std::size_t>(4096, output_data.size() - offset);
          !WriteExactly(&client, std::span(output_data.data() + offset, count),
                        screen_deadline)) {
        return;
      }
    }
    if (request->kind == 2) {
      log->Append(symbian::agent::AgentLogCode::kStatusRead);
    } else if (request->kind == 1) {
      negotiated = true;
    }
  }
  client.Close(absl::Now() + kControlDeadline).IgnoreError();
}

class AgentService final {
 public:
  explicit AgentService(
      std::shared_ptr<const agent_service::AgentIdentity> identity)
      : identity_(std::move(identity)),
        listener_([this](absl::StatusOr<TcpClient> result) {
          OnAccept(std::move(result));
        }) {}

  absl::Status Start() {
    if (SYMBIAN_AGENT_PRIVATE_PROFILE) {
      TraceStartup("C32 inventory queued");
      auto inventory = worker_.PostFiber(
          [] {
            TraceStartup("C32 inventory worker entered");
            TraceSerialPorts();
          },
          kWorkerStackBytes);
      if (auto result = inventory.ResultIfReady(); result.has_value()) {
        return result->status();
      }
#if SYMBIAN_AGENT_WIFI_PAIRED
      auto task = worker_.PostFiber(
          [stop = stopping_, log = log_, identity = identity_] {
            while (!stop->load()) {
              if (auto host = DiscoverHost(identity->key());
                  host.ok() && !stop->load()) {
                link_phase.store(LinkPhase::kDialing);
                if (auto client = TcpClient::ConnectIpv4(
                        *host, kHostPort, absl::Now() + absl::Seconds(3));
                    client.ok() && !stop->load()) {
                  link_phase.store(LinkPhase::kAuthenticating);
                  Serve(std::move(*client), identity, log.get());
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
#else
      return absl::OkStatus();
#endif
    }
    ABSL_RETURN_IF_ERROR(listener_.EnableWorkerSharing());
    return listener_.ListenIpv4({127, 0, 0, 1}, kAgentPort);
  }

  void OnAccept(absl::StatusOr<TcpClient> result) {
    if (stopping_local_) {
      return;
    }
    if (result.ok()) {
      // The bounded queue closes a rejected client through its captured owner.
      worker_.PostFiber(
          [client = std::move(*result), log = log_,
           identity = identity_]() mutable {
            Serve(std::move(client), identity, log.get());
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
  std::shared_ptr<const agent_service::AgentIdentity> identity_;
  std::shared_ptr<symbian::agent::AgentLogRing> log_ =
      std::make_shared<symbian::agent::AgentLogRing>();
  symbian::concurrency::WorkerExecutor worker_{4};
  symbian::api::connectivity::ActiveTcpListener listener_;
  std::shared_ptr<std::atomic<bool>> stopping_ =
      std::make_shared<std::atomic<bool>>(false);
  bool stopping_local_ = false;
};

const agent_service::ResidentPanelOptions kPanel{
    .app_uid = 0xe0000a31u,
    .property_category = agent_service::kPropertyCategory,
    .foreground_key = agent_service::kRaisePanelKey,
    .caption = "Development Agent",
    .heading = "AGENT",
    .state = SYMBIAN_AGENT_PANEL_STATE,
    .back_label = "BACK",
    .stop_label = "STOP",
    .heading_provider = SYMBIAN_AGENT_WIFI_PAIRED ? AgentHeading : nullptr,
};

}  // namespace

absl::Status RunAgentService() {
  TraceStartup("RunAgentService entered");
  ABSL_ASSIGN_OR_RETURN(auto identity,
                        agent_service::AgentIdentity::OpenOrCreate());
  TraceStartup("Agent identity ready");
  AgentService service(identity);
  TraceStartup("AgentService constructed");
  std::string panel_state = "PAIR " + identity->pairing_code();
  agent_service::ResidentPanelOptions panel = kPanel;
  if (SYMBIAN_AGENT_PRIVATE_PROFILE) {
    panel.state = panel_state.c_str();
  }
  std::atomic<bool> stop_requested{false};
  std::thread ui_thread;
  absl::Status panel_status = absl::OkStatus();
  const absl::Status result = symbian::api::system::RunActiveService(
      agent_service::kPropertyCategory, agent_service::kStopServiceKey,
      [&] {
        TraceStartup("service start entered");
        absl::Status status = service.Start();
        TraceStartup(status.ok() ? "service start completed"
                                 : status.ToString());
        return status;
      },
      [&] { service.CloseListener(); },
      [&] {
        TraceStartup("active service ready");
        ui_thread = std::thread([&] {
          TraceStartup("panel thread entered");
          panel_status =
              agent_service::RunResidentPanel(panel, &stop_requested);
          TraceStartup(panel_status.ok() ? "panel returned normally"
                                         : panel_status.ToString());
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
  TraceStartup(result.ok() ? "active service returned normally"
                           : result.ToString());
  stop_requested.store(true);
  if (ui_thread.joinable()) {
    ui_thread.join();
  }
  // A second menu launch raises the panel of the resident instance.
  if (symbian::NativeErrorFromStatus(result) == symbian::native_error::kInUse) {
    return agent_service::RequestResidentPanelForeground(
        agent_service::kPropertyCategory, agent_service::kRaisePanelKey);
  }
  ABSL_RETURN_IF_ERROR(result);
  return panel_status;
}

extern "C" int RuntimeMain() {
  TraceStartup("RuntimeMain entered");
  symbian::api::system::DebugLog("agent: entering RuntimeMain");
  constexpr std::array<std::u16string_view, 1> logs{
      u"E:\\Others\\agent_startup.txt"};
  return symbian::api::system::RunWithFailureHandler(
      []() -> absl::Status {
        const absl::Status result = RunAgentService();
        if (!result.ok()) {
          TraceStartup(result.ToString());
        }
        return result;
      },
      {.caption = "Agent startup error", .log_paths = logs});
}
