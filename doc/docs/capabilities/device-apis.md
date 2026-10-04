# Modern on-device APIs

This is the plan for application-facing C++ APIs built on the target SDK. The
public surface lives under `cpp/symbian/api/include/symbian/api/`; the SDK
exports it as `<symbian/api/...>` with separate opt-in `Symbian::<Component>`
archives.
Implementation files live beside it under `cpp/symbian/api/<component>/`.
The SDK owns any legacy Symbian header, descriptor, leave and request-status
translation in separate native bridge translation units. Applications receive
typed C++ values and `absl::Status` or `absl::StatusOr` and can keep using the
SDK's C++20 standard library.

## Component sequence

| Component | First useful application operation | Evidence before exposing more |
| --- | --- | --- |
| `system` | Read tick and fast counters, including their actual period or frequency | Packaged linked probe on both ARM targets and emulator CPU backends; wrap-specific tests remain open |
| `storage` | Own file and directory cursors; stream reads and entries, and write with explicit creation and flush | File Server import and read/write checks in a disposable emulator instance; large files and device behavior remain open |
| `power` | Read independent power-good, external-supply and qualitative battery fields | HAL import and status checks; device validation and change notifications remain open |
| `display` | Query primary pixel geometry and optional physical twips | HAL import and positive geometry check; orientation change and multiple screens remain open |
| `sensors` | Enumerate available channels and read timestamped samples | Header/ordinal and capability checks, subscription cancellation and overflow controls |
| `media` | Audio playback and recording with owned buffers | Audio service contracts, timing, format negotiation and cancellation |
| `camera` | Typed ECam camera-count discovery, without activating hardware | Opening, preview and capture need a verified leave boundary and named-firmware callback, permission and buffer-lifetime evidence |
| `connectivity` | Connect and exchange bounded IPv4 TCP data on a worker | Asynchronous listener, cancellation, deadlines, bearer observation and physical-device checks remain open |

`system`, `power`, `display`, `storage`, `camera` and `connectivity` have public
headers and separate archives. The sensor and media rows are planned
components, not claimed device features.
Their separate directories reserve source boundaries; add a public header and
archive only after a native service contract has been verified. Keep
device-specific facts as unknown until measured. No flashing, erasure,
bootloader, partition, OTP, calibration or hardware-recovery operation belongs
in this application API.

## API rules

- Group APIs by a device capability rather than by historical server class.
  Do not expose raw descriptors, `TRequestStatus`, cleanup-stack ownership or
  native error integers in public headers.
- Keep synchronous queries bounded. Long-running or event-driven operations
  return SDK `Future`/`Task` values, own their native requests through
  cancellation and drainage, and integrate with the existing `EventExecutor`.
  Any compute-heavy continuation is explicitly placed on its worker executor.
- Open native handles and allocate buffers only when a request needs them.
  Define closure and cancellation before adding subscriptions. Use an explicit
  capability or availability result instead of a guessed fallback value.
- Test host-side data/error mapping with GTest, then compile and execute the
  actual bridge on ARMv5T and ARMv6 under both emulator CPU backends. Device
  evidence is a separate gate and must name its model and firmware.
- Package public headers and both architecture archives in the SDK manifest,
  with digests and a CMake target. A packaged-header compile and linked
  emulator probe are required before calling a component usable.

## Initial `system` slice

`ReadTickCounter()` and `ReadFastCounter()` return typed readings from the
already verified `User::TickCount`/`UserHal::TickPeriod` and
`User::FastCounter`/kernel HAL frequency bridges. Their counts are 32-bit and
wrap; the API does not present them as wall time or an indefinitely increasing
uptime. A missing or zero period/frequency returns a status. The implementation
does not include historical platform headers and performs no device write.

Example application CMake:

```cmake
target_link_libraries(my_app PRIVATE Symbian::System)
```

Example C++:

```cpp
#include "symbian/api/system/counters.h"

auto reading = symbian::api::system::ReadFastCounter();
if (reading.ok()) {
  const auto ticks_per_second = reading->ticks_per_second;
}
```

## Additional device slices

`Symbian::Power` reports three independent optional fields. A missing HAL
attribute stays unknown, and an unknown battery enumeration is never coerced
to a percentage. `Symbian::Display` requires valid primary HAL pixel dimensions;
physical twip dimensions remain optional. Both queries are snapshots, not
subscriptions. A legacy `hal.dll` proxy and the original EPL-licensed HAL
headers remain inside the SDK native boundary.

`Symbian::Storage` owns a File Server session and file or directory subsession
per open handle. Its movable C++ owners close those resources exactly once.
`ReadOnlyFile::ReadAt` uses a caller-owned `std::span<std::byte>` rather than an
allocating return buffer. `DirectoryReader::Next` returns one UTF-16 entry or
end-of-stream instead of building an unbounded list. Calls can block on I/O:
run the whole handle lifetime on one worker, including destruction. The first
profile supports offsets below 2 GiB. `WritableFile` makes create, open and
replace explicit, writes directly from caller memory and exposes native
`Flush`; parent directory creation is a separate call. It does not expose
erase or raw partition operations. The native bridge owns `RFs`, `RFile`, `RDir` and
descriptors; applications do not include those headers.

`FileCopy` advances by one reusable 32 KiB chunk per `Step()` and yields
`CopyProgress` after each step. A worker can forward that progress to a UI or
stream; a consumer need not wait for the whole copy. `Cancel()` atomically
requests a stop from another thread, checked before and between the read,
write and flush. Directory listing is likewise entry-at-a-time and has its own
cross-thread stop request. Current
native File Server operations are synchronous, so an already-running request
may delay terminal cancellation. A fixed wall-clock cancellation bound needs
verified asynchronous File Server requests and cancellation drainage; the
current contract makes no such claim.

Application CMake can opt into only the required archives:

```cmake
target_link_libraries(my_app PRIVATE Symbian::Power Symbian::Display
  Symbian::Storage)
```

For example, a worker can keep one read-only file open for repeated reads:

```cpp
#include <array>
#include <cstddef>
#include <utility>
#include "symbian/api/storage/storage.h"

auto opened = symbian::api::storage::ReadOnlyFile::Open(u"C:\\Data\\sample.bin");
if (opened.ok()) {
  auto file = std::move(*opened);
  std::array<std::byte, 4096> block{};
  auto bytes_read = file.ReadAt(0, block);
  // Use the first *bytes_read bytes only when bytes_read.ok().
}
```

## Next native contracts

Connectivity should separate observation from connection creation: a snapshot
must not silently enable mobile data or change access points. Sensors need
typed channel metadata, timestamped samples and a bounded subscription whose
drop policy is visible. Media and camera should own native streams and buffers
with explicit cancellation and drainage; decoding, encoding and image work
belong on a worker, while event-thread completions remain bounded. Camera
preview and still capture must keep capability and privacy state explicit.
These are design gates, not exported APIs yet.
