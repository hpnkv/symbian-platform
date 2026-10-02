#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "symbian/native_status.h"

extern "C" int RuntimeMain() {
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
