# Guest C++ runtime

The SDK supplies application-linked libc++, compiler-rt and native adapters
for ARMv5T and ARMv6. Guest libraries use C++20, AAPCS soft-float, 16-bit
`wchar_t`, and disabled exceptions and RTTI. Use the SDK's headers, generated
configuration and selected archive together; host libraries are not guest
runtime dependencies.

## Select a runtime profile

| CMake target | Facilities and requirements |
| --- | --- |
| `Symbian::Runtime` | Core libc++ containers, ownership, clocks, allocation and compiler helpers |
| `Symbian::Streams` | Core runtime plus classic C/POSIX locale and string-stream formatting |
| `Symbian::LegacyEka2` | ARMv5T older-ROM streams runtime with SDK C, threading, math, UTF-8, file, formatting and time adapters; select `SYMBIAN_RUNTIME_LEGACY_EKA2=ON` for SDK component targets |
| `Symbian::Threads` | Threaded runtime profile; firmware must provide the selected `libpthread.dll` imports |
| `Symbian::AbseilStatusOr` | Status, StatusOr, Abseil logging, Cord, time and flat hash maps with the matching streams runtime |
| `Symbian::NativeAtomics64` | Alternative runtime using native EUSER 64-bit atomics; requires matching firmware exports |
| `Symbian::Stackless` | Future/Task composition, native request owners and event/worker executors |
| `Symbian::Fibers` | ARM-backed fibers and an explicitly pumped, thread-affine scheduler |

Link the runtime selected by your component targets. Do not link both
`Symbian::Runtime` and `Symbian::Streams` into one image: their libc++
configurations must match their archives. A DLL using an Abseil-based component
sets `RUNTIME_TARGET Symbian::Streams`; see
[native library targets](../reference/project-libraries.md).

The default architecture for new projects is ARMv6. Select ARMv5T when the
target requires it. The modern runtime requires EKA2; the
[EKA1 process profile](../guides/eka1.md) is separate.

## Allocation and failure policy

Startup creates the Symbian thread heap before calling application code.
Ordinary, array, sized, nothrow and over-aligned allocation are provided.
Ordinary allocation failure terminates with `KErrNoMemory` (-4); nothrow
allocation returns null. Zero-size allocation requests at least one byte, and
sizes above the native signed-size limit are rejected. Container growth does
not turn allocation failure into a recoverable `StatusOr`.

For a download scratch buffer whose allocation failure should be recoverable,
choose nothrow allocation explicitly and enforce an application limit:

```cpp
#include <cstddef>
#include <memory>
#include <new>

#include "absl/status/statusor.h"

absl::StatusOr<std::unique_ptr<std::byte[]>> AllocateDownloadBuffer(
    std::size_t bytes) {
  if (bytes == 0 || bytes > 64 * 1024) {
    return absl::InvalidArgumentError("Buffer must be 1–65536 bytes");
  }
  auto buffer =
      std::unique_ptr<std::byte[]>(new (std::nothrow) std::byte[bytes]);
  if (!buffer) {
    return absl::ResourceExhaustedError("No download buffer");
  }
  return buffer;
}
```

This handles this allocation only; later container or Status payload allocations
still follow the ordinary allocation policy.

The default `SYMBIAN_RUNTIME_MIMALLOC=ON` profile uses mimalloc 3.5.3 with
process-owned `RChunk` storage, reserve/commit/decommit support and pthread
cleanup. It reserves 8 MiB per virtual arena and defaults to a 64 MiB total
chunk address budget. Configure `SYMBIAN_MIMALLOC_ADDRESS_BUDGET_MIB` from
16 to 512. A source build needs `SYMBIAN_MIMALLOC_SOURCE` pointing to the
pinned checkout. `SYMBIAN_RUNTIME_MIMALLOC=OFF` uses the original `RHeap`
adapter. The native 64-bit atomic runtime uses that adapter and cannot be
combined with the mimalloc profile.

Thread registration must pair entry and exit. Direct `RThread::Create` threads
must remain unregistered unless their owner supplies both; a native thread
exit does not necessarily run pthread cleanup. General C++ `thread_local`,
ELF TLS relocations and DLL TLS destruction are unsupported.

Fatal libc++ preconditions and invalid no-exceptions thread operations exit
with `KErrArgument` (-6). These exits are runtime contract failures, rather
than recoverable application argument errors. Check ownership and input ranges
before calling operations such as `std::thread::join()`.

## Abseil logging and failure reports

`LOG`, `DLOG`, `VLOG`, `DVLOG`, the rate-limited `LOG_*` family and `CHECK`
use the installed Abseil logging implementation. SDK executables initialize
Abseil logging at startup and route its messages to `DebugLog`, which writes
to the platform debug sink and retains the newest messages for failure reports.
To receive logs in an additional destination, implement the `absl::LogSink`
observer, keep it alive while registered,
and pair `absl::AddLogSink(&sink)` with `absl::RemoveLogSink(&sink)` before its
destruction. `LOG(...).ToSinkOnly(&sink)` is available for one statement.
Sinks run synchronously and must avoid locks held by logging callers. Do not
call `absl::InitializeLog()` again in an SDK executable.

SDK executables link the failure handler automatically. A failed `CHECK` or
`CHECK_OK` saves its message and recent SDK `DebugLog` lines to
`C:\private\<app UID>\failure.txt`. A foreground failure shows the scrollable
report with Copy Logs and Exit controls; a background failure only writes the
file. The failure view is also available explicitly through
`symbian::api::system::ShowFailureReport` and `RunWithFailureHandler`.
Dragging moves the content by the pointer's distance in screen pixels, paced to
the reported display refresh rate. Pending input is applied before painting;
host trackpad momentum retains its original deltas. Clipped native text is
composed into the background bitmap before
one window blit, so scrolling cannot present a background-only update. Redraws
reuse that complete bitmap without a second text pass or self-invalidation.
Bitmap writes wait for outstanding Window Server reads; font and bitmap graphics
contexts are cached. Rendering uses Window Server,
so the platform can use its accelerated compositor without requiring EGL or
GLES support in the failing process. Acceleration depends on the device and
Window Server implementation; the same path works on older software renderers.
`examples/failure_handler_app` logs varied messages and deliberately fails a
`CHECK_EQ` for quick on-device testing of scrolling, copying and exit.

```cpp
absl::Status Main();

int main() {
  CHECK_OK(Main());
  return 0;
}
```

## Standard library and C services

The core archive supplies strings, vectors, smart pointers, hash containers,
threads, mutexes, condition variables, clocks and error categories from pinned
LLVM sources. Function-section linking includes only reachable functions.
Compiler-rt supplies integer division/remainder, ARM EABI wrappers, aligned
memory helpers, soft-float arithmetic/conversions/comparisons and ARMv5-safe
wide-arithmetic helpers. The 32-bit EABI divide-by-zero hook exits with
`KErrArgument`; do not rely on recoverable divide-by-zero behavior.

Memory copies use the selected EUSER imports. Some operations also require
firmware C/POSIX libraries: hash-table growth can import `ceilf` from
`libm.dll`, formatting uses `vsnprintf`, error messages use `strerror_r`, and
thread yield uses `sched_yield`. The SDK's `stdarg_e.h` preserves Clang's ARM
variadic-call convention; use it ahead of the historical OpenC header.

The installed ARMv5T `Symbian::LegacyEka2` archive combines the source
options `SYMBIAN_RUNTIME_LOCAL_MATH`, `LOCAL_C_STRING`, `LOCAL_C_STDLIB`,
`LOCAL_POSIX_TIME`, `NATIVE_PTHREAD`, `LEGACY_EUSER` and the streams locale.
Set `SYMBIAN_RUNTIME_LEGACY_EKA2=ON` when configuring an installed-SDK
application so Abseil and component targets select that same archive. Use the
software SDL targets on firmware without EGL/GLES2. The older firmware
checks covered E71/RM-346 and 6120c/RM-243 in EKA2L1; other ROM export
tables need their own check.

The legacy calendar parser uses fixed C-locale names; its `%Z` adapter recognizes
`STD`/`DST`, without general timezone or locale support. A separate experiment
with the modern firmware's native Open C parser faulted while parsing named
weekdays/months. That locale-dependent path remains unresolved and is excluded
from the passing logging/codec probes.

The C adapter provides reached integer conversion, sort, float conversion,
ASCII ctype, error text, allocation, bounded formatting and UTF-8 functions.
The C allocation family and C++ allocation share the RHeap backend; `realloc`
preserves the original block on failure. The profile has no inherited C
environment, so `getenv` returns null. File and stream calls use checked
`estlib.dll` ordinals, not Open C. The local math functions cover binary32 and
binary64 values, including subnormal ties, without promising C `errno` or
floating-point exception flags. The POSIX clock adapter accepts realtime and
monotonic clocks, and `sysconf` accepts the processor-count form. Unsupported
forms return `EINVAL`; sleeps round up to microseconds and use `RTimer`.

Native pthread adapters cover the reached libc++ and SDL calls, including
EUSER threads, semaphore-based locks/conditions and bounded per-thread key
state with exit destructors. ARM SWP avoids newer EUSER atomic imports absent
from the older ROMs. Worker threads share their creator's process heap; the
heap owner must outlive worker allocations. This is not a complete POSIX
thread or `errno` implementation. Incremental UTF-8 decoding, invalid input
and thread isolation passed the older-ROM probe. A 16-bit `wchar_t` cannot
hold a supplementary scalar: `mbrtowc` returns `EILSEQ` for it, while
`wcrtomb` accepts a surrogate pair across calls.

The file and time migration path uses public SDK owners and helpers:

| Existing boundary code | Application-facing API |
| --- | --- |
| `RFs`/`RFile`/`RDir` session and descriptor management | `Symbian::Storage`: `ReadOnlyFile`, `WritableFile`, `DirectoryReader`, `FileCopy` |
| C `mbstate_t` loops or hand-written descriptor conversion | `<symbian/api/text/utf8.h>` `Utf8ToUtf16` and `Utf16ToUtf8` |
| `User::After` frame wait | `<symbian/api/time/sleep.h>` `SleepFor`, or `FramePacer` for a presentation loop |
| Native counters and HAL period | `Symbian::System` counter readings and the SDK monotonic clock |
| `sprintf` into an unbounded buffer | `absl::StrFormat` from `Symbian::AbseilStatusOr`, or bounded `snprintf`/`std::ostringstream` from `Symbian::Streams`/`Symbian::LegacyEka2` |

The public text helpers validate complete UTF-8/UTF-16 input and retain
embedded NULs and supplementary scalars. They return `InvalidArgument` for
malformed input. The older C multibyte adapter remains available for
incremental work; keep an explicit `mbstate_t` and check `EILSEQ`. Its
`mbrtowc` path is limited to BMP values. `SleepFor` currently returns no error and
returns immediately if native timer creation fails; use a fallible timer owner
when a missed wait must be reported.
The installed ARMv5T runtime consumer exercises `absl::StrFormat`, stream
formatting and the public strict UTF-8 conversion on E71 and 6120c emulator
fixtures. Formatting allocates a `std::string` and follows the ordinary fatal
allocation-failure policy; it is not a fallible `StatusOr` operation.

`Symbian::Streams` supports classic locale, C/POSIX locale names and
`std::ostringstream`. It does not provide arbitrary named locales, file
streams, general wide I/O, filesystem, random-device or timezone services.
`std::promise` and `std::future` require an exception ABI that this profile
does not supply; use the SDK Future/Task APIs instead.

## Clocks and atomics

`std::chrono::system_clock` reads the firmware real-time clock. The SDK steady
clock uses the nanokernel counter and its HAL-reported period, with the ordinary
tick and its period as fallback. It extends the 32-bit count and clamps
out-of-order cross-thread samples. Keep reads less than half a counter wrap
apart (about 24.9 days at a 1 ms period); suspension behavior is device-specific.

Use `absl::Time` for absolute real-world deadlines and `absl::Duration` for
relative delays. Native timer owners convert an accepted absolute deadline
once to monotonic waiting, so later wall-clock corrections do not move it.
A native fast counter is optional for profiling; check its frequency, direction
and power cost before using it.

32-bit compiler atomics call ordered EUSER operations. Default 64-bit atomics
serialize through a process-owned `RFastLock`. `Symbian::NativeAtomics64`
uses native exports instead: RM-807, RM-675 and RM-609 profiles provide the
selected exports; RM-243 and RM-346 do not. Other firmware needs a compatible
export table and implementation. Use an object's standard `is_lock_free()`
query; compiler target macros can disagree with the linked runtime.

## Storage, relocations and lifetime

EXE code and writable data relocate independently. The converter supports
initialized `.data`, zero-initialized `.bss` and typed pointers to either
mapping, with a combined data/BSS limit of 1 MiB. Startup runs `.init_array`
after heap setup and finalizers before exit. DLL startup runs process-attach
constructors; dynamic load and close use `RLibrary`. A bounded two-worker
local-static initialization test passed on E71/RM-346 and 6120c/RM-243 using
the ROM's `__cxa_guard_acquire`/`release` imports. Recursive initialization,
guard abort after a failed initializer, ELF TLS and general DLL teardown
ordering remain unsupported or unverified.

The local GOT is word-aligned, limited to 1,024 words, and requires retained
`R_ARM_GOT_PREL` symbol coverage. Imported function pointers resolve through
validated PLT entries. Bounded imported object pointers in read-only code
storage are supported; the ECam native-leave boundary publishes its
`drtaeabi` exception type-info pointer through that path. Imported object
storage outside the validated relocation forms remains unsupported. Internal
or hidden PC-relative references across the code/data
mappings are rejected; keep DLL globals default-visible when GOT references
are needed. Unsupported sections and relocations cause conversion errors.

Historical Symbian placement-new declarations conflict with modern libc++
`<new>`. Keep original platform headers in separate translation units and
cross into modern code through a narrow adapter. Never pass modern strings
or containers to frozen OS DLL C++ interfaces; use native descriptors at
those interfaces. See [C++ application guidance](cpp.md).

## Asynchronous operations

Use [SDK concurrency](concurrency.md) for Futures, Tasks, timers, property
watches, channels and fibers. Native handles, request statuses and buffers
remain owned until completion or cancellation drainage. Inline continuations
can run on the completing thread; dispatch UI work to its event owner and
blocking work to a worker. The runtime provides no preemption for a blocked
fiber or an indefinitely waiting worker.

## Build the runtime from source

Follow [source preparation](../guides/source-prerequisites.md) for platform
headers and the matching Clang/LLD tools. Keep external LLVM sources outside
version control:

```sh
git clone --depth 1 --branch llvmorg-23.1.2 --filter=blob:none --sparse \
  https://github.com/llvm/llvm-project.git research/upstream/llvm-project
git -C research/upstream/llvm-project sparse-checkout set libcxx libcxxabi \
  compiler-rt cmake runtimes llvm/cmake
```

The SDK source build applies its target patches and generates the matching
libc++ configuration. Use [SDK installation](../reference/project-configuration.md)
to export headers, archives and import proxies. The
[runtime guide](../guides/guest-runtime-source.md) shows a complete example.

## Older EKA2 ROMs and timed waits

The ARMv5T legacy EUSER profile can link software SDL2/SDL3 without Open C
`libc.dll`, `libpthread.dll` or `libm.dll`. It uses verified older `estlib.dll`
ordinals for bounded file and stream compatibility, with SDK-owned format,
float and calendar adapters. E71/RM-346 and 6120c/RM-243 emulator runs
exercise SDL2 input, rendering and normal AppArc exit; SDL3 is checked on
6120c. This does not establish compatibility with every ESTLIB caller or a
physical older handset.

`#include "symbian/api/time/sleep.h"` provides
`symbian::api::time::SleepFor(std::chrono::nanoseconds)`. Its native adapter
waits on a thread-relative timer request. SDL2/SDL3 frame pacing and the
legacy POSIX `nanosleep` use this path because a positive `User::After` wait
stalled an E71 SDL guest after its first frame. Timer creation failure returns
without sleeping; callers that need error reporting should use a fallible
timer or task API. The distinction between firmware and emulator behavior
for `User::After` remains under investigation.
