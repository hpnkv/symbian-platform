# Guest C++ runtime

The visible SDK includes an ARM/Thumb context-switch
prerequisite in each runtime archive. A 16 KiB stack preserves C++ local
objects across two switches and balances heap cells in 8/8 ARMv5T/ARMv6 ×
Dyncom/Dynarmic normal/changed controls. It is not yet a public fiber API:
stack bounds/guards, floating-point state, native leave/TRAP safety,
scheduling, cancellation, joining and A11 `thread::` lock/event semantics
still require implementation and execution tests.

The installed SDK includes a bounded `symbian::concurrency::NativeTimer`
request owner in `<symbian/concurrency/native_timer.h>`. It wraps one
thread-relative `RTimer`, rejects invalid or overlapping arms and cancels and
drains a pending request before close. Calls must remain on the creating OS
thread. The caller's event loop owns the shared request semaphore; this class
does not wait or dispatch callbacks. It remains the low-level C1 request owner;
the separate `TimerPump` provides the tested Task completion path.

The newer `symbian::concurrency::TimerPump` profile in
`<symbian/concurrency/timer_pump.h>` converts these native timer completions
to A11-derived `Task` results carrying real guest `absl::StatusOr<Unit>`.
`ScheduleAfter(absl::Duration)` and `ScheduleAt(absl::Time)` use the original
guest Abseil types. `absl::Time` remains real-world absolute time: at
registration, `ScheduleAt` subtracts `absl::Now()` once, then uses monotonic
elapsed time and native relative timer arms of at most signed 32-bit
microseconds. Later wall-clock adjustments do not move an accepted request.
Past and infinite-past deadlines complete on the next event turn; infinite
future remains pending until cancelled or closed. Negative relative delays
fail with `InvalidArgument`; unrepresentably distant finite absolute deadlines
fail with `OutOfRange`. Long finite waits are segmented into native arms.
A future wall-clock alarm that tracks later clock changes needs a separate
contract. A worker may call
`Task::Cancel()`; an owned, coalesced request signal wakes the event thread,
which alone cancels the `RTimer` and publishes completion. `DispatchReady()`
may invoke `OnReady` inline and limits each pass to 64 completions by default.
The pump admits at most 64 pending timers by default; its constructor accepts
a different limit. Saturation returns an already-ready Task with
`kResourceExhausted`, and capacity returns after event-thread dispatch. The
number is a conservative SDK admission policy, not a measured Symbian handle
limit; choose it for the application's workload and available memory.
The application must inspect all Window Server statuses and timer results
before calling `Park()`; `Park()` is the combined loop's sole request-semaphore
wait. The ordinary `symbian init` starter now executes this combined loop on
Dynarmic and Dyncom when drive Z provides `libpthread.dll`; the counter
example uses the same pump for a delayed marker and Reset cancellation.
`init --firmware` selects the portable profile when that DLL is absent, and
Run checks actual imports before emulator startup. Broader native service
requests and full C1/C2 controls remain open. Public timer durations use the pinned guest Abseil time type; the
steady-clock conversion remains inside the adapter.

`PropertyWatch` in `<symbian/concurrency/property_watch.h>` now owns an
integer Publish & Subscribe property and one outstanding change request. Its
`Next()` returns `Future<int>`. `Open`, `Next`, `Set`, `DispatchReady` and
`Close` stay on the event thread; worker cancellation sets an atomic request
flag and coalesces a wakeup. The native bridge owns the `TRequestStatus`,
property handle and result storage, and drains cancellation before close.
The request entry is removed before a possibly inline `OnReady` callback; the
guest test re-subscribes from that callback. Pass `TimerPump::WakeCallback()`
to this owner and dispatch it alongside the timer pump before the one
`TimerPump::Park()` wait. The separate `EventMailbox` provides bounded
cross-thread callbacks for this same event thread. Its `Enqueue` is an
affinity dispatch route, not A11's shared-pool `Post`; `DispatchReady(budget)`
executes callbacks outside its lock and defers reentrant work to a later turn.
The generated timer GUI now uses the mailbox. Both owners and the mailbox
passed the 8-case ARMv5T/ARMv6 × Dyncom/Dynarmic normal/changed matrix.

An unresolved `Future::Await` now parks cooperatively when called from a
guest `thread::Fiber`, and still returns `FailedPrecondition` outside a fiber
on the event thread. Guest `thread::Mutex` and `CondVar` park fibers on
contention/signal/timeout; OS-thread callers retain blocking behavior.
`thread::SleepFor` also parks a fiber, but blocks an OS worker outside one.
`thread::Scheduler` is explicitly pumped and pinned to its creating OS
thread. Its optional `SchedulerPolicy` selects a ready fiber and receives
cross-thread wake notifications outside the scheduler lock. The event owner
must arrange `RunReady` turns and one native wait armed to `NextDeadline`;
that integration is not yet shipped. `EventMailbox` uses the channel's
nonblocking operations. A11 `Select`, `PermanentEvent`, cancellation trees,
joining and pool scheduling remain open. The channel, Future/Task,
TaskGroup and mailbox headers are shared with the host CMake target; their
primitive backends differ.
The guest condition variable includes pinned libc++'s separate destructor
translation unit. Timed waits use an accepted absolute `absl::Time` converted
to bounded monotonic remaining time; an early libc++ `no_timeout` without an
actual signal is ignored using a signal generation. Long-wait CPU cost and
physical-device timing remain to be measured. `CondVar` now matches A11's
boolean convention: `true` means timeout, `false` means signal.

An installed-SDK probe now uses A11-derived `TaskGroup` to join two native
timer Tasks, then cancel a group of three. It passed normal and changed-result
controls on ARMv5T/ARMv6 × Dyncom/Dynarmic. Completion and cancellation still
run through the one event-thread pump; TaskGroup introduces no worker.

A guest runtime is now a first-class SDK requirement. The initial maintained
subset executes real LLVM libc++ `std::string` and `std::vector<int>` with the
Symbian heap. It is separate from the host's Abseil/native Python runtime.

## Current executable contract

`cpp/symbian/runtime/` compiles for ARMv5T or ARMv6/AAPCS soft-float with
C++20, exceptions and RTTI disabled, and the SDK-required 16-bit wchar_t ABI.
The runtime is mostly PIC, while the tested libc++ thread-local-data source
and application target use absolute data references that E32 can relocate.
ARMv6 is the new-project default; each profile has its own runtime archive.
The opt-in execution matrix covers both CPU profiles on Dynarmic and Dyncom,
including a real ARMv6 `REV` instruction and the
ARMv5T software sequence. These results establish emulator and format support,
not CPU compatibility for an unidentified physical phone.
LLVM 23.1.2 sources are pinned at
`85ac560262434c9ccfc0c183ec22d4138ed647fb`. Their maintained
source edits are the Symbian atomic lock-free query, monotonic-clock and
ARMv5-safe compiler-rt patches; see
research/llvm/README.md.
CMake generates the target configuration from the original LLVM template and
uses its original assertion handler. All consumers use that same configuration
and the private `std::__symbian` namespace. Pthread-backed threads are now
enabled where the selected ROM supplies `libpthread.dll`; localization,
filesystem, wide characters, random-device and timezone services remain
unverified.

The static library builds original `string.cpp`, `new_helpers.cpp`,
`memory.cpp`, `thread.cpp`, `mutex.cpp`, `condition_variable.cpp`,
`future.cpp` and `hash.cpp` with function sections. The linker collects functions reached by
the probe. Symbian adapters supply ordinary/array/sized/nothrow and over-aligned
new/delete,
strlen/memcmp, and compiler-generated memory operations through real EUSER
memcpy/memmove/memset. The hash-table path imports the selected ROM's `ceilf`
from `libm.dll`; unrelated applications do not need that symbol. No host
libc++ library is linked. Original compiler-rt sources provide signed/unsigned
32-/64-bit division and remainder, EABI wrappers, aligned memcpy aliases,
soft-float arithmetic/conversion/comparison, 64-bit multiplication and the
ARMv5T-safe leading-zero helper needed by wide division. The 32-bit EABI
divide-by-zero hook exits with KErrArgument;
wide divide-by-zero is not yet an acceptance case.
Original ARM soft-double add/subtract/multiply/divide, comparisons and
conversions now execute on ARMv5T/ARMv6 × Dyncom/Dynarmic. A maintained LLVM
patch replaces five instructions unavailable before ARMv6T2 while retaining
the original algorithms and source identity. This closes compiler ABI
dependencies in the pinned Abseil StatusOr link; the later bounded guest
Status/StatusOr and flat-hash-map execution result is described below.
OpenC's preserved headers provide target C declarations, with the C99 and
long-long
feature macros enabled. The upstream notices remain in the ignored sources.

Startup creates the real thread heap before running the program. Ordinary new
terminates the guest with KErrNoMemory (-4) on failure; nothrow new returns
nullptr. Zero-size allocation requests one byte. Sizes above TInt's maximum
are rejected before conversion to the SDK API. Fatal libc++ precondition paths
exit with KErrArgument (-6). They do not masquerade as successful results.
This is an explicit termination policy, not recoverable allocation through
StatusOr. The bounded guest Abseil StatusOr port uses this failure policy.

A real `std::unordered_set<int>` now inserts, reserves, looks up and erases
elements with balanced Symbian heap cells on ARMv5T/ARMv6 and both emulator CPU
backends. A changed-result control fails as expected. This validates libc++'s
`__next_prime` and the required soft-float/`ceilf` path on the named RM-807
fixture; it does not yet establish guest Abseil `flat_hash_map`. Older ROMs
without `libm.dll` need an SDK math implementation before this particular
hash-table path can run there.

The original libc++ `chrono.cpp` now supplies `std::chrono::steady_clock` and
`system_clock` through a guarded Symbian adaptation. The named RM-807 ROM's
`clock_gettime(CLOCK_REALTIME)` works, but `CLOCK_MONOTONIC` returns
`EINVAL`. The steady clock therefore reads the native nanokernel counter and
its HAL-reported microsecond period; it falls back to the ordinary tick and
its reported period if that HAL capability is unavailable. A process-wide
64-bit atomic extends the 32-bit counter across wrap and clamps out-of-order
cross-thread samples. EKA2L1's missing nanokernel-period HAL operation was
added as an ordered emulator patch. This is a bounded monotonic-deadline
prerequisite, not a guarantee about suspension behavior or a gap of half a
counter wrap between reads (about 24.9 days at a 1 ms nanokernel period).
`User::FastCounter()` remains an optional profiling source: its
frequency is device-specific, and the original Symbian implementation warns
that it can cost power and may require hardware activation. The emulator's
fast-counter frequency and count rate now agree in a one-second guest probe.
Two real `std::thread` readers also execute 2,048 simultaneous clock samples
each, followed by 128 release/acquire handoffs that require the next thread's
sample to be no earlier than the published one. Normal and changed-result
controls passed on both ARM profiles and emulator CPU backends from source
and the installed SDK. The probe checks that thread/clock use returns to its
initial Symbian heap-cell count. It does not measure clock-call latency or
prove the half-wrap/suspension contract.

The converter and loader tests cover bounded writable EXE and DLL `.data`/`.bss`
and typed code/data fixups. A frozen-export DLL with simple initialized/BSS
globals executes twice in each fresh process on Dyncom and Dynarmic; the loader
allocates and resets its per-process storage. LLVM must emit relocatable GOT
references for those globals. Internal/hidden data references that cross the
independently relocated code/data mappings still produce an explicit conversion
error. Bounded EXE global constructors/destructors and C++ DLL process-attach
constructors now run through SDK entries. A dynamic `RLibrary` client also
loads, looks up, closes and observes a DLL destructor on the named Belle
fixture. TLS, local-static guards and general DLL lifetime remain open. The
historical `EPOCALLOWDLLDATA` was a converter opt-in, not the
storage or lifetime implementation; SDK users need no MMP directive.

There is a real SDK conflict: `e32cmn.h`/`e32cmn.inl` declare and define placement
new with incompatible exception specifications and duplicate definitions when
combined with modern libc++ `<new>`. The SDK adapter therefore has its own
translation unit. Only a small C ABI crosses into modern C++ code; upstream
headers are unchanged. Do not pass modern strings or containers into frozen
Symbian DLL C++ interfaces. Platform descriptors belong at those boundaries.

## Reproduce

Prepare the GUI source headers first using WALKTHROUGH.md. Preserve LLVM in an
ignored checkout:

```sh
git clone --depth 1 --branch llvmorg-23.1.2 --filter=blob:none --sparse \
  https://github.com/llvm/llvm-project.git research/upstream/llvm-project
git -C research/upstream/llvm-project sparse-checkout set libcxx libcxxabi \
  compiler-rt cmake runtimes llvm/cmake
```

Generate the selected EUSER proxy using the native frozen-DEF reader:

```sh
uv run python - <<'PY'
from pathlib import Path
from symbian.sdk import build_import_proxy
symbols = [
    '_ZN4User11InitProcessEv', '_ZN4User4ExitEi', '_ZN4User9InvariantEv',
    '_ZN8UserHeap15SetupThreadHeapEiR24SStdEpocThreadCreateInfo',
    '_ZN4User5AllocEi', '_ZN4User4FreeEPv', '_ZN4User15CountAllocCellsEv',
    'memcpy', 'memmove', 'memset',
]
build_import_proxy(
    Path('research/upstream/kernelhwsrv/kernel/eka/eabi/euseru.def'),
    symbols, 'euser.dll', Path('.symbian/runtime-sdk/euser'),
    compiler='/opt/homebrew/opt/llvm/bin/clang++',
    linker='/opt/homebrew/bin/ld.lld',
)
PY
uv run symbian build --project examples/runtime_probe \
  --output .symbian/runtime-probe \
  --compiler /opt/homebrew/opt/llvm/bin/clang++ \
  --linker /opt/homebrew/bin/ld.lld
```

These are the current macOS paths; CMake cache variables accept other preserved
LLVM/OpenC/SDK locations. The ARM target triple is host independent. Linux host
wheel verification does not yet establish this guest runtime on Linux.

The build publishes a matching ELF/E32 pair after two byte-identical independent
builds and records consumed source/header digests. Build reports intentionally
keep runtime verification false: separate execution evidence supplies that gate.
The original Symbian validator accepts this image and rejects bad-CRC and
negative-heap controls.

```sh
SYMBIAN_RUNTIME_WORKSPACE="$PWD" \
SYMBIAN_RUNTIME_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ \
  uv run pytest -q symbian/tests/test_guest_runtime.py
```

This requires the patched EKA2L1 and digest-pinned Delight golden instance from
WALKTHROUGH.md. Tests build ordinary success/failure, global-nothrow/GOT success
and changed-GOT-value failure variants, writable-storage success and a
changed-initialized-data control, global construction/destruction, a changed
64-bit arithmetic control, a native `RFastLock` contract probe, and 32-bit
atomic success/changed-result controls, parent/worker atomic controls,
unique/shared/weak ownership controls, `std::thread` controls, and the isolated
exception-metadata positive/changed controls. Nineteen variants span both ARM
profiles and Dyncom/Dynarmic (76 cases); the current checkpoint ran the eight
metadata cases and a separate typed-throw build-negative gate, not the entire
matrix. With
`SYMBIAN_EKA2L1_ORACLES_BUILD` set, four additional tests run
the independent original checksum/whole-image validator on both storage images. Every instance is a
fresh copy; ROM/EUSER digests are checked before and after. Success exercises
20 rounds of heap-backed string append/insert/erase/substr and growth to 1,024
vector elements, checks the independent arithmetic result, and verifies the
SDK heap allocation count returns to its initial value. Nothrow oversized
allocation returns nullptr. A representable 4 MiB request exceeds the image
header's 1 MiB heap limit: nothrow returns nullptr and ordinary new exits with
-4 through the real SDK allocation failure. Default allocation alignment, a 512-byte aligned allocation and symmetric
sized deletion are checked, including oversized aligned nothrow failure.
Volatile operands independently exercise signed and unsigned 32-/64-bit
division/remainder. The changed-wide control exits -126.
The fast-lock bridge creates a real local `RFastLock`, checks uncontended
`Poll`, verifies that polling while held returns `KErrTimedOut`, and exercises
`Signal`, `Wait` and close. It uses the real EUSER imports and a separate SDK
header translation unit. This proves one uncontended/held-lock path on the
named firmware; it does not yet prove cross-thread contention or an A11 mutex.
The atomic bridge supplies Clang's observed `__atomic_*` ARMv5T and
`__sync_*` ARMv6 32-bit entry points by calling the original EUSER ordered
add, compare-and-swap and exchange operations and acquire load. Its probe
checks return values, failed-CAS expected-value update and a changed-result
control on both CPU profiles and emulator backends. This verifies one
single-thread execution path; cross-thread and 64-bit controls are below.
A separate bounded `RThread` probe uses `Create`, `Logon`, `Resume`,
`WaitForRequest`, `ExitReason`, `ExitType` and `Close`. The worker enters through
`UserHeap::SetupThreadHeap(ETrue, ...)` and its callback; global initializers
run only on the process primary thread. Parent and worker each perform 2,000
atomic increments after a shared ready barrier. All eight normal/changed-result
cases pass across ARMv5T/ARMv6 and Dyncom/Dynarmic. This verifies one join and
shared-state path on the named ROM; it does not establish TLS teardown, general
thread cancellation, cross-thread allocation/free, A11 futures or fibers. The
Belle ROM calls SVC 0x34 for `RThread::ExitReason`; the fourteenth emulator patch
maps that observed call to its existing handler.
The 64-bit compiler ABI now has load/store/exchange/CAS/fetch-add and observed
`__sync` operations. The default archive serializes these through a
process-owned EUSER `RFastLock`; `Symbian::NativeAtomics64` is a complete
alternate archive using the ROM's native 64-bit exports and a matching
`euser-native64` proxy. Select the latter only for a firmware whose exports
and behavior are verified. The imported RM-807, RM-675 and RM-609 EUSERs have
byte-identical 64-bit entry prefixes with LDREXD/STREXD; cross-thread probes
pass on both emulator backends. Dyncom needed the ordered
`dyncom-strexd-value.patch` to write
the assembled high and low words. The original V5/V6 Symbian implementation
can mask interrupts instead, so an import by name is not proof of lock-free
behavior on a different ROM. RM-243 and RM-346 lack the selected EABI EUSER
64-bit ordinals; do not select the native profile for those ROMs. The SDK's
pinned libc++ header patch routes
`atomic::is_lock_free()` and `atomic_ref::is_lock_free()` to the selected
runtime profile; Clang's ARMv6 builtin gave the wrong answer for the default
lock-backed archive. These tests use an actual parent/worker contention path,
but wider thread counts, memory-order litmus tests and other ROMs remain open.
The eight atomic cases also pass when the temporary probe links the visible
SDK's prebuilt `Symbian::Runtime` archive, standard EUSER proxy and installed
headers instead of rebuilding the runtime from the checkout.
The runtime now builds original libc++ `memory.cpp`, `thread.cpp`, `mutex.cpp`,
`condition_variable.cpp` and `future.cpp` with the threaded configuration.
`std::unique_ptr`, `std::shared_ptr` and `std::weak_ptr` pass construction,
move/copy, lock/expiry and allocation-cell cleanup controls. A `std::thread`
using the preserved `libpthread.dll` joins after 4,000 concurrent atomic
increments, copies/releases shared ownership and destroys a moved
`unique_ptr` in its worker. Normal and changed-result controls pass 16
ARM-profile/backend cases, and the same 16 pass against a newly exported SDK's
prebuilt archive, headers and standard proxies. This is one bounded thread and
ownership path, not general pthread, TLS cleanup or A11 Future/Task support.
The runtime uses original libc++ `system_error.cpp` for failed thread
operations; its no-exceptions path reaches the SDK's fatal abort adapter.
An invalid join on an empty `std::thread` exited with -6 on both ARM profiles
and emulator CPU backends. Such errors do not become a recoverable
`StatusOr`. `std::this_thread::yield()` uses the selected ROM's
`sched_yield` import from `libc.dll` on the tested RM-807 profile.
The E32 converter rejects PIC PC-relative code/data references because code
and data relocate independently. The tested ownership source and original
libc++ thread-local-data translation unit use absolute data fixups; the SDK's
application runtime target supplies this compile choice. The bounded PIC GOT
probe remains a separate control. A wider vtable/data relocation contract
still needs validation.
`std::promise`/`std::future` currently reach missing `exception_ptr`,
`logic_error` and error-category symbols at link. The SDK's guest profile
defaults to exceptions off; an exception-capable guest application profile is
required and planned. This does not alter the host library policy. The standard
linker scripts still discard ARM unwind tables. An isolated exception probe now
preserves `.ARM.extab`/`.ARM.exidx`, emits the four-word Symbian descriptor,
converts its typed relocations and sets/validates the E32 header's descriptor
offset. A no-throw C++ catch/cleanup path executes on both ARM targets and CPU
backends with a changed-result control (eight cases). This verifies metadata,
loader and one normal control-flow path, not a thrown exception. A real typed
throw builds an additional `R_ARM_GLOB_DAT` import for `_ZTIi`, which the
function-only ordinal resolver explicitly rejects. Real throw/catch,
destructor unwinding, failure policy and standard futures remain blocked until
that data-import contract and exception ABI pass guest execution.
Logs and native kernel exit records are retained in Pytest artifacts.

## Local GOT and model acquisition

`std::nothrow` as an external global now works. The converter accepts one
word-aligned `.got` table of at most 1,024 words inside the RX mapping. Every
word must exactly match a defined object/function named by a retained
R_ARM_GOT_PREL record, including Thumb state. It emits typed E32 text/data fixups for the
GOT words and preserves the already linked PC-relative references. LLVM's
synthesized GOT has no separate ABS32 records; ignoring that table would leave
unrelocated addresses. Undefined/preemptible external objects, TLS, unknown GOT
forms, unclaimed words and unsupported symbol kinds are rejected. No section
is discarded to make conversion succeed.

The global-nothrow variant also dereferences a separately compiled constant
(2026) and calls a Thumb function through the GOT. A changed constant exits
with -113 on both backends. This establishes actual relocation-dependent
execution, rather than relying only on the empty nothrow tag. Native GTests
cover missing/duplicate/malformed tables, orphan words, missing symbol coverage,
pointer state, section bounds and rejection of writable storage in RX.
The original Symbian validator accepts the new image and passes its six
corruption/contract controls. Generated apps now create their model with
`new (std::nothrow)` and close acquired Window Server resources on null before
returning KErrNoMemory; both-backend 4 MiB model-allocation controls exit -4.
Later container growth still uses the fatal ordinary-allocation policy.

## Writable EXE storage

The runtime probe and new-project linker script now use an independent,
non-executable RW PT_LOAD at linked address `0x20000000`. This is a link-time
address; the loader independently relocates the actual code and data mappings.
The converter transports initialized bytes and BSS size, emits typed 0x1000
(text target) / 0x2000 (data target) fixups in both source mappings, and bounds
combined initialized data/BSS to 1 MiB. Local code GOT slots may target either
mapping, including BSS; Thumb function-pointer state is preserved.

The maintained probe reads separately defined initialized storage, checks all
64 BSS words start at zero, follows initialized pointers to data/BSS and a
Thumb function, mutates data/BSS and observes the changes. It succeeds on both
firmware-backed CPU backends; changing the initial value exits -115 on each.
The independent original whole-image validator accepts both images. Native
controls reject overlapping virtual/file mappings, out-of-range or wrongly
classified pointers, malformed tables, RX writable sections, cross-mapping
PC-relative references, TLS and malformed constructor arrays. Pure-BSS transport is also
covered by a native format fixture; no separate pure-BSS guest execution is
claimed. The inspector reports data/BSS sizes/base and both typed fixup lists.

This enables constant-initialized mutable EXE globals and bounded C++ globals.
The SDK startup calls `.init_array` after the thread heap exists and runs
`__cxa_finalize`/`.fini_array` before exit. Real `std::string` globals and
heap-cell balance pass both-backend execution. A C++ DLL's SDK entry runs its
constructor on the actual process-attach call list; eight execution cases
include a changed-constructor failure control across both ARM profiles and CPU
backends. A dynamic client additionally exercises `RLibrary::Load`, two frozen
ordinal lookups, `Close` and destructor delivery into client-owned memory; an
absent DLL returns a load error. Local-static guards, TLS and broader DLL
lifetime remain unverified.
The bounded profile is not a full general GOT, data-import or DLL loader ABI.

## C varargs and C++ error categories

OpenC's original `stdarg_e.h` assumes an older pointer-based ARM `va_list` and
overrides Clang's builtin varargs macros. The runtime now places an SDK-owned
header first in the guest include path and exports it as
`include/config/stdarg_e.h`. The guest probe calls the firmware's real
`libc.dll` `vsnprintf` with register and stacked arguments, including a
64-bit integer. Normal and changed-result controls passed 16 source-tree
and 16 fresh-SDK cases over ARMv5T/ARMv6 and both emulator CPU backends. This
establishes the tested C variadic call shape; other variadic APIs need their
own controls.

The archive builds LLVM libc++'s original `error_category.cpp` and
`system_error.cpp`. `std::error_code`, `generic_category()`, a message obtained
through `strerror_r`, and default error-condition comparison execute in the
same 16-case matrices with negative controls. Those runtime objects use absolute
data references for the E32 relocation model. The initial PIC build failed
the converter's cross-mapping check, which remains enforced. The visible SDK
was refreshed with the tested archive and header; its 2,503-file digest
manifest verifies, and the previous SDK is retained.
The default SDK profile still excludes locale and iostream. A separate
`Symbian::Streams` archive now builds original pinned libc++ locale, ios,
ostream, iostream and strstream sources with an SDK-owned C/POSIX locale
adapter. It supports classic locale and bounded `std::ostringstream` output;
it does not provide arbitrary named locales, file streams or general wide I/O.
Selected 16-bit-wide libc++ and OpenC functions now support the tested Abseil
closure. Applications select this target *instead of* `Symbian::Runtime`
because its libc++ configuration and archive must match. Both ARM targets and
both emulator CPU backends passed normal and changed-result controls from
source and from a fresh sealed SDK export (eight cases per matrix). The
adapter's invalid-name/mask/category checks also run in the guest. The visible
SDK was promoted after digest audits (2,535 sealed files); the predecessor is
retained at `~/dev/symbian-sdk-before-streams-20261001`.
The A11-pinned Abseil `Status`, `StatusOr`, `Cord` payloads and
`flat_hash_map<std::string, int>` now execute on ARMv5T/ARMv6 and both emulator
CPU backends, using real pinned source. A matched installed profile exposes
`Symbian::AbseilStatusOr`, 384 headers and 43 compiled closure archives for
each ARM target. It uses the wide-enabled `Symbian::Streams` runtime, not the
default archive. The original `LowLevelAlloc` uses an SDK `RChunk` page backend:
a 130,000-byte allocation, free, and explicit arena deletion pass a bounded
guest control. This does not verify memory pressure, cross-thread page release,
all Abseil components, or general ELF/C++ TLS.
A separate original-SDK `RChunk` probe passes bounded create, write, grow and
close controls on both ARM targets and emulator CPU backends, from source and
against the installed runtime archive. Page commit/decommit, cross-thread
ownership remain open.

An internal `SymbianRuntimePageCreate`/`Close` bridge now creates exact-sized,
page-aligned, process-owned `RChunk` storage, retains the handle in opaque
metadata, validates errors before publication and releases the handle and
metadata together. Its first same-thread guest controls pass; cross-thread
closing, mmap-style lookup and memory pressure remain unverified. A separate
`pthread` key probe checks worker isolation and a thread-exit destructor.
This does not establish general C++ `thread_local`, ELF TLS relocations or DLL
TLS destruction. A pinned Abseil guard avoids one `__tls_get_addr` dependency
by using Abseil's `GetTID()` fallback, currently `pthread_self()` on Symbian.
Runtime process exits use internal `SymbianRuntimeExitReason` names for
out-of-memory (`KErrNoMemory`) and fatal runtime-contract failures
(`KErrArgument`); the latter is a process exit category, not a recoverable
argument error API.

The ELF/E32 converter now relocates a bounded global pointer to an imported
function through its validated PLT entry. A probe initializes a writable
pointer to EUSER `memmove`, calls it through a local GOT reference from a
separate translation unit, and checks copied bytes. It passed ARMv5T and
ARMv6 on Dyncom and Dynarmic using both source and the installed SDK archive;
changed ELF controls reject a data-object relocation and a false PLT symbol
value. This does not enable imported data objects such as exception typeinfo.
The visible SDK was refreshed with this converter change after an 11-case
installed-SDK matrix and digest audit; its previous tree is retained.
The earlier isolated original-libc++ `ostringstream`/classic-locale prototype
exposed an unrelocated linker-generated
interworking thunk. The converter now fixes LLD's exact named eight-byte ARM
long thunk, and the original stream image executes on both backends. A
maintained ARMv5T/ARMv6 thunk probe executes on both emulator backends and a
mutated target fails conversion. Other unrelocated thunk forms remain open.

## Actual A11 source adoption

The original A11 thread/concurrency source closure is staged unchanged under
`cpp/symbian/concurrency/upstream`. Its 46 digests and 92 local include edges
pass the CMake source-check target, five Python integrity/negative tests and
comparison with the original Git pin. The upstream Boost/forced-unwind backend
is still unavailable under the SDK's default no-exceptions policy. Threaded
shared ownership and one `std::thread` path pass bounded controls. Guest
Broader guest Abseil, complete TLS/thread ownership and the no-exceptions
A11 fiber backend remain execution gates. Guest exceptions are a separate
opt-in profile requirement. Generated applications have a tested opt-in
Status/timer profile; the original gui_app has not been migrated. See the
component README and PLAN.md C0–C5.

The guest now also exports a bounded `Symbian::Stackless` target with a licensed
`symbian::concurrency` adaptation of Promise/Future/Task, `OnReady`, `Then`, ordered
`JoinAll`, bounded reentrant `DriveInline` and a `TaskGroup` that completes
after every child settles. The API uses original guest `absl::StatusOr<T>` and
`absl::Status` directly.
Callbacks run inline on the completing thread, including when registering on a
ready Future. `Cancel()` requests producer action and does not complete the
Future itself; destroying an incomplete Promise publishes a cancelled result.
There is no `Await`, worker scheduler or fiber in this profile, so UI work must
remain nonblocking and callbacks must handle their own thread affinity. The
normal and changed-result guest probe passed eight architecture/backend cases
against the latest sealed SDK candidate. The separate timer integration also
passed eight cases. Full A11 parity and native OS request adapters beyond the
timer remain open.

## Remaining gates

This does not yet provide all libc++ or a general hosted C++20 runtime.
Priorities are thread-safe local statics, TLS/DLL detach and unload lifetime,
complete compiler-rt builtins,
new-handler policy, and a broader C-service surface.
Broaden the bounded Abseil Status/StatusOr and flat_hash_map ports, then port
nlohmann::json with explicit
failure behavior. Threads must use A11's thread library with a Symbian backend;
there is no second scheduler. Coroutines, broader atomics, general formatting,
file streams and filesystem each require separate execution/conformance gates.
Matched physical
808 execution remains unverified.

LLVM's [vendor configuration guidance](https://libcxx.llvm.org/VendorDocumentation.html)
requires a matching configuration for the library and its users. Runtime
features will be enabled together with their implementation and tests.
