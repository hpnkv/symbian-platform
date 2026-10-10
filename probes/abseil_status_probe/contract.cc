#include <atomic>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/log/globals.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "absl/log/log_sink.h"
#include "absl/log/log_sink_registry.h"
#include "absl/status/status.h"
#include "absl/status/status_macros.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "symbian/native_status.h"

#if defined(SYMBIAN_ABSEIL_CODEC_FACTORIES)
#include "symbian/http/http2.h"
#include "symbian/websocket/websocket.h"

absl::Status CheckCodecFactories() {
  namespace http = symbian::http;
  namespace ws = symbian::websocket;
  ABSL_ASSIGN_OR_RETURN(auto source, http::Http2::Create(http::Role::kClient));
  http::Http2 client(std::move(source));
  if (!source.ended() || source.Feed({}).ok()) {
    return absl::InternalError("moved HTTP/2 owner retained a session");
  }
  ABSL_ASSIGN_OR_RETURN(auto assigned,
                        http::Http2::Create(http::Role::kClient));
  assigned = std::move(client);
  ABSL_ASSIGN_OR_RETURN(auto server,
                        http::Http2::CreateUnique(http::Role::kServer));
  ABSL_ASSIGN_OR_RETURN(const auto request, assigned.TakeOutput());
  ABSL_RETURN_IF_ERROR(server->Feed(request));
  ABSL_ASSIGN_OR_RETURN(const auto reply, server->TakeOutput());
  ABSL_RETURN_IF_ERROR(assigned.Feed(reply));
  if (!server->peer_settings_received() || !assigned.peer_settings_received()) {
    return absl::InternalError("moved HTTP/2 settings exchange failed");
  }
  ABSL_ASSIGN_OR_RETURN(auto socket, ws::WebSocket::Create(ws::Role::kServer));
  ws::WebSocket moved(std::move(socket));
  ABSL_ASSIGN_OR_RETURN(auto replacement,
                        ws::WebSocket::Create(ws::Role::kServer));
  replacement = std::move(moved);
  if (!socket.closed() || !moved.closed() || socket.Feed({}).ok()) {
    return absl::InternalError("moved WebSocket owner retained state");
  }
  ABSL_ASSIGN_OR_RETURN(const auto output, replacement.TakeOutput());
  if (output.empty()) {
    return absl::InternalError("moved WebSocket lost settings");
  }
  ABSL_ASSIGN_OR_RETURN(auto unique,
                        ws::WebSocket::CreateUnique(ws::Role::kServer));
  ABSL_ASSIGN_OR_RETURN(const auto unique_output, unique->TakeOutput());
  return unique_output.empty()
             ? absl::InternalError("unique WebSocket lost settings")
             : absl::OkStatus();
}
#endif

#if defined(SYMBIAN_ABSEIL_FOREGROUND_CHECK)
#include "symbian/api/display/window_surface.h"
#include "symbian/api/system/clipboard.h"

absl::Status ForegroundMain() {
  ABSL_ASSIGN_OR_RETURN(
      auto window,
      symbian::api::display::WindowSurface::Create("Foreground failure probe"));
  constexpr std::u16string_view text = u"Clipboard probe: \u03bb \u2603";
  ABSL_RETURN_IF_ERROR(symbian::api::system::CopyTextToClipboard(text));
  ABSL_ASSIGN_OR_RETURN(const auto copied,
                        symbian::api::system::ReadTextFromClipboard());
  if (copied != text) {
    return absl::DataLossError("clipboard round trip changed text");
  }
  for (int index = 0; index < 40; ++index) {
    LOG(INFO) << "foreground recent log " << index;
  }
  return absl::InternalError("foreground guest reporting probe");
}
#endif

class ProbeLogSink final : public absl::LogSink {
 public:
  void Send(const absl::LogEntry&) override {
    messages.fetch_add(1, std::memory_order_relaxed);
  }

  std::atomic<int> messages{0};
};

class RegisteredProbeLogSink final {
 public:
  RegisteredProbeLogSink() { absl::AddLogSink(&sink); }

  ~RegisteredProbeLogSink() { absl::RemoveLogSink(&sink); }

  ProbeLogSink sink;
};

absl::StatusOr<int> CheckStatusMacros(absl::StatusOr<int> input) {
  ABSL_ASSIGN_OR_RETURN(const int value, std::move(input));
  ABSL_RETURN_IF_ERROR(absl::OkStatus());
  return value + 1;
}

int main() {
#if defined(SYMBIAN_ABSEIL_CODEC_FACTORIES)
  CHECK_OK(CheckCodecFactories());
#endif

#if defined(SYMBIAN_ABSEIL_FOREGROUND_CHECK)
  CHECK_OK(ForegroundMain());
#endif
#ifdef SYMBIAN_ABSEIL_FAILED_CHECK
  CHECK(false) << "fatal guest reporting probe";
#endif
#if !defined(SYMBIAN_SDK_LOGGING_INITIALIZED)
  absl::InitializeLog();
#endif
  RegisteredProbeLogSink registered_sink;
  absl::SetGlobalVLogLevel(1);
  LOG(INFO) << "guest log";
  DLOG(INFO) << "guest debug log";
  VLOG(1) << "guest verbose log";
  DVLOG(1) << "guest debug verbose log";
  for (int index = 0; index < 4; ++index) {
    LOG_FIRST_N(INFO, 2) << "guest limited log";
    LOG_EVERY_N(INFO, 2) << "guest periodic log";
    DLOG_FIRST_N(INFO, 2) << "guest limited debug log";
    VLOG_FIRST_N(1, 2) << "guest limited verbose log";
  }
  LOG_EVERY_N_SEC(INFO, 1) << "guest time-limited log";
  LOG_IF(WARNING, true) << "guest conditional log";
  if (registered_sink.sink.messages.load(std::memory_order_relaxed) < 10) {
    return -215;
  }
  CHECK_OK(absl::OkStatus());
  CHECK_EQ(2 + 2, 4);
  CHECK_STRCASEEQ("Symbian", "sYMbIaN");
  DCHECK_NE(1, 2);
  if (auto good = CheckStatusMacros(41); !good.ok() || *good != 42) {
    return -213;
  }
  if (auto bad = CheckStatusMacros(absl::DataLossError("macro propagation"));
      bad.ok() || bad.status().code() != absl::StatusCode::kDataLoss) {
    return -214;
  }
  absl::Status error = absl::InvalidArgumentError("guest status");
  if (error.ok() || error.code() != absl::StatusCode::kInvalidArgument ||
      error.message() != "guest status") {
    return -201;
  }
  error.SetPayload("guest.key", absl::Cord("payload"));
  auto payload = error.GetPayload("guest.key");
  if (!payload.has_value() || payload->Flatten() != "payload") {
    return -205;
  }
  absl::Status copied = error;
  if (copied.message() != "guest status" ||
      !copied.GetPayload("guest.key").has_value()) {
    return -206;
  }
  absl::StatusOr<int> failed(error);
  if (failed.ok() ||
      failed.status().code() != absl::StatusCode::kInvalidArgument) {
    return -202;
  }
  absl::StatusOr<std::string> success(std::string("hello"));
  if (!success.ok() || *success != "hello") {
    return -203;
  }
  absl::StatusOr<std::string> moved(std::move(success));
  if (!moved.ok() || moved.value() != "hello") {
    return -207;
  }
  absl::flat_hash_map<std::string, int> numbers;
  numbers.emplace("first", 17);
  numbers.emplace("second", 23);
  if (numbers.size() != 2 || numbers.at("first") != 17 ||
      numbers.erase("second") != 1 || numbers.contains("second")) {
    return -208;
  }
  const absl::Duration interval = absl::Milliseconds(75);
  if (absl::ToInt64Microseconds(interval) != 75000 ||
      absl::ToInt64Milliseconds(interval + absl::Milliseconds(25)) != 100) {
    return -209;
  }
  const absl::Time now = absl::Now();
  if (absl::ToUnixSeconds(now) < 1000000000 ||
      absl::ToUnixSeconds(now + absl::Seconds(2)) - absl::ToUnixSeconds(now) !=
          2) {
    return -210;
  }
  if (absl::FormatTime("%Y-%m-%dT%H:%M:%SZ", absl::UnixEpoch(),
                       absl::UTCTimeZone()) != "1970-01-01T00:00:00Z") {
    return -211;
  }
#if defined(SYMBIAN_RUNTIME_LEGACY_EUSER)
  // Exercise the C locale parser supplied by the older runtime. The standard
  // profile imports its parser from Open C instead.
  absl::Time parsed;
  std::string parse_error;
  if (!absl::ParseTime("%a %b %e %I:%M:%S %p %Y", "Thu Jan 1 12:00:00 AM 1970",
                       absl::UTCTimeZone(), &parsed, &parse_error) ||
      parsed != absl::UnixEpoch()) {
    return -216;
  }
#endif
  const absl::Status native = symbian::StatusFromNativeError(
      symbian::native_error::kNoMemory, "allocate pages");
  if (native.code() != absl::StatusCode::kResourceExhausted ||
      symbian::NativeErrorFromStatus(native) !=
          symbian::native_error::kNoMemory ||
      symbian::NativeErrorFromStatus(error) !=
          symbian::native_error::kArgument ||
      !symbian::StatusFromNativeError(0, "noop").ok()) {
    return -212;
  }
#ifdef SYMBIAN_ABSEIL_CHANGED_STATUS
  return -204;
#else
  return 0;
#endif
}
