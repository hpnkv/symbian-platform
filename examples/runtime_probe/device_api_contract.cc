#include "abi.h"
#include "absl/time/time.h"
#include "symbian/api/display/display.h"
#include "symbian/api/power/power.h"
#include "symbian/api/system/counters.h"
#ifdef SYMBIAN_RUNTIME_DEVICE_API_CAMERA
#include "symbian/api/camera/camera.h"
#endif
#ifdef SYMBIAN_RUNTIME_DEVICE_API_STORAGE
#include <array>
#include <utility>

#include "symbian/api/storage/storage.h"
#endif

extern "C" int SymbianRuntimeDeviceApiProbe() {
  const auto tick = symbian::api::system::ReadTickCounter();
  if (!tick.ok() || tick->period <= absl::ZeroDuration() ||
      absl::ToInt64Microseconds(tick->period) !=
          SymbianRuntimeTickPeriodMicros()) {
    return -430;
  }
  const auto fast = symbian::api::system::ReadFastCounter();
  if (!fast.ok() || fast->ticks_per_second == 0 ||
      fast->ticks_per_second !=
          static_cast<unsigned int>(SymbianRuntimeFastCounterFrequency())) {
    return -431;
  }
  const auto geometry = symbian::api::display::ReadPrimaryDisplayGeometry();
  if (!geometry.ok() || geometry->width_pixels <= 0 ||
      geometry->height_pixels <= 0) {
    return -432;
  }
  const auto power = symbian::api::power::ReadPowerSnapshot();
  if (power.ok()) {
    if (!power->power_good && !power->external_power && !power->battery) {
      return -433;
    }
  } else if (power.status().code() != absl::StatusCode::kUnimplemented &&
             power.status().code() != absl::StatusCode::kUnavailable) {
    return -434;
  }
#ifdef SYMBIAN_RUNTIME_DEVICE_API_STORAGE
  auto opened =
      symbian::api::storage::ReadOnlyFile::Open(u"Z:\\resource\\psui.r01");
  if (!opened.ok()) {
    // Keep the mapped error category visible in the guest exit reason.
    return -450 - static_cast<int>(opened.status().code());
  }
  auto file = std::move(*opened);
  const auto size = file.Size();
  if (!size.ok() || *size < 4) {
    return -436;
  }
  std::array<std::byte, 4> bytes{};
  const auto read = file.ReadAt(0, bytes);
  if (!read.ok() || *read != bytes.size()) {
    return -437;
  }
  auto directory =
      symbian::api::storage::DirectoryReader::Open(u"Z:\\resource");
  if (!directory.ok()) {
    return -438;
  }
  auto entry = directory->Next();
  if (!entry.ok() || !entry->has_value() || entry->value().name.empty()) {
    return -439;
  }
  directory->Cancel();
  if (directory->Next().status().code() != absl::StatusCode::kCancelled) {
    return -440;
  }
  const auto created = symbian::api::storage::CreateDirectories(
      u"C:\\private\\E0000813\\device-api-probe\\");
  if (!created.ok()) {
    return -460 - static_cast<int>(created.code());
  }
  const std::array<std::byte, 4> payload = {std::byte{'S'}, std::byte{'D'},
                                            std::byte{'K'}, std::byte{'!'}};
  {
    auto writable = symbian::api::storage::WritableFile::Open(
        u"C:\\private\\E0000813\\device-api-probe\\roundtrip.bin",
        symbian::api::storage::WriteMode::kCreateNew);
    if (!writable.ok()) {
      return -470 - static_cast<int>(writable.status().code());
    }
    if (!writable->WriteAt(0, payload).ok() || !writable->Flush().ok()) {
      return -480;
    }
  }
  auto written = symbian::api::storage::ReadOnlyFile::Open(
      u"C:\\private\\E0000813\\device-api-probe\\roundtrip.bin");
  if (!written.ok()) {
    return -490 - static_cast<int>(written.status().code());
  }
  std::array<std::byte, 4> roundtrip{};
  const auto result = written->ReadAt(0, roundtrip);
  if (!result.ok() || *result != roundtrip.size() || roundtrip != payload) {
    return -500;
  }
  auto copy = symbian::api::storage::FileCopy::Open(
      u"C:\\private\\E0000813\\device-api-probe\\roundtrip.bin",
      u"C:\\private\\E0000813\\device-api-probe\\copied.bin",
      symbian::api::storage::WriteMode::kCreateNew);
  if (!copy.ok()) {
    return -510 - static_cast<int>(copy.status().code());
  }
  const auto copy_progress = copy->Step();
  if (!copy_progress.ok() || !copy_progress->complete ||
      copy_progress->bytes_copied != payload.size()) {
    return -520;
  }
#endif
#ifdef SYMBIAN_RUNTIME_DEVICE_API_CAMERA
  const auto cameras = symbian::api::camera::DiscoverCameras();
  if (!cameras.ok() &&
      cameras.status().code() != absl::StatusCode::kUnimplemented &&
      cameras.status().code() != absl::StatusCode::kUnavailable) {
    return -530 - static_cast<int>(cameras.status().code());
  }
#endif
  return 0;
}
