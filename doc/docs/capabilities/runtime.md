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
| `Symbian::Threads` | Threaded runtime profile; firmware must provide the selected `libpthread.dll` imports |
| `Symbian::AbseilStatusOr` | Status, StatusOr, Cord, time and flat hash maps with the matching streams runtime |
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

The source workspace has an experimental `SYMBIAN_RUNTIME_LOCAL_MATH=ON`
profile for older firmware. It supplies the seven reached C math functions
locally and selects EUSER's C memory primitives before Open C. A software
SDL2 ARMv6 image built with this profile and
`SYMBIAN_ARKANOID_GPU=OFF` has no `libm.dll`, EGL or GLES2 imports. It still
has 66 `libc.dll` and 19 `libpthread.dll` import slots with mimalloc enabled;
an RHeap build has 65 and 18 respectively. Neither image can load on the
tested E71 or 6120c firmware yet. The local math functions cover IEEE binary32
and binary64 values, including subnormal ties; they do not promise C `errno`
or floating-point exception flags. The profile is currently a source-workspace
experiment, not an installed SDK runtime variant.

The source workspace also offers `SYMBIAN_RUNTIME_LOCAL_C_STRING=ON`. It links
seven original BSD/Nokia Open C string and wide-string routines from the
preserved source checkout. On the same RHeap software SDL2 build, this reduces
`libc.dll` imports from 65 to 58 slots (58 to 51 distinct dynamic symbols).
The original notices remain with those sources. File I/O, formatting, locale,
clock and pthread imports remain, so the image still cannot load on E71 or
6120c. This option is experimental and is not in the installed SDK.

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
constructors; dynamic load and close use `RLibrary`. Thread-safe local-static
guards, TLS and general DLL teardown ordering are unsupported.

The local GOT is word-aligned, limited to 1,024 words, and requires retained
`R_ARM_GOT_PREL` symbol coverage. Imported function pointers resolve through
validated PLT entries. Imported data objects, including exception typeinfo,
are unsupported. Internal or hidden PC-relative references across the code/data
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
