# Modern on-device APIs

The SDK provides application-facing C++ APIs built on the target OS services. The
public surface lives under `cpp/symbian/api/include/symbian/api/`; the SDK
exports it as `<symbian/api/...>` with separate opt-in `Symbian::<Component>`
archives.
Implementation files live beside it under `cpp/symbian/api/<component>/`.
The SDK owns any legacy Symbian header, descriptor, leave and request-status
translation in separate native bridge translation units. Applications receive
typed C++ values and `absl::Status` or `absl::StatusOr` and can keep using the
SDK's C++20 standard library.

## Available components

| CMake target | Application operation | Restrictions |
| --- | --- | --- |
| `Symbian::System` | Read tick and fast counters with their period or frequency | 32-bit counters wrap; these are not wall time |
| `Symbian::Storage` | Stream files and directories, write and flush, copy in chunks | Synchronous I/O; offsets below 2 GiB |
| `Symbian::Power` | Read power-good, external-supply and qualitative battery fields | Optional fields can be unknown; snapshot only |
| `Symbian::Display` | Query primary pixel geometry and optional physical twips | Snapshot only; no multiple-screen or orientation subscription |
| `Symbian::Camera` | Discover the available camera count | No camera opening, preview or capture |
| `Symbian::Connectivity` | Resolve hosts, connect, accept and exchange IPv4 TCP data | Worker-owned blocking operations; active accepts require an active scheduler |

Sensors and media are planned components with no public headers or archives.
The [component guides](apis/index.md) describe each available API's ownership,
threading and error handling.

## Read a native counter

`ReadTickCounter()` and `ReadFastCounter()` return typed readings from the
`User::TickCount`/`UserHal::TickPeriod` and
`User::FastCounter`/kernel HAL frequency bridges. Their counts are 32-bit and
wrap; the API does not present them as wall time or an indefinitely increasing
uptime. A missing or zero period/frequency returns a status. The implementation
does not include historical platform headers and performs no device write.

Example application CMake:

```cmake
target_link_libraries(my_app PRIVATE Symbian::System)
```

Use a [counter reading](apis/system.md#measure-a-short-interval) to measure a
short operation, preserving its period and handling wrap rather than treating a
bare count as wall time.

## Additional device slices

`Symbian::Power` reports three independent optional fields. A missing HAL
attribute stays unknown, and an unknown battery enumeration is never coerced
to a percentage. `Symbian::Display` requires valid primary HAL pixel dimensions;
physical twip dimensions remain optional. Both queries are snapshots, not
subscriptions. A legacy `hal.dll` proxy and the original EPL-licensed HAL
headers are used by the SDK bridge.

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
may delay terminal cancellation. Cancellation has no fixed wall-clock bound.

Application CMake can opt into only the required archives:

```cmake
target_link_libraries(my_app PRIVATE Symbian::Power Symbian::Display
  Symbian::Storage)
```

For a file preview, use the [bounded read helper](apis/storage.md#read-only-a-bounded-prefix).
For persistence, [save a draft](apis/storage.md#save-a-small-draft) with explicit
replacement and error handling. Both helpers keep the file lifetime on the
calling worker and report native failures to the application.
