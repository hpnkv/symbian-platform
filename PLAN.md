# Mission: Revive the Nokia 808 / Symbian Belle Development Platform for 2026

You are working on a long-running engineering and research project whose goal is to create a **modern, macOS-native development stack for the Nokia 808 PureView and the Symbian Belle ecosystem**, suitable for serious application development, system-component development, reverse engineering, debugging, experimentation, and potentially eventually developing an alternative operating system for the hardware.

The Nokia 808 remains the initial physical target. The SDK's application and
emulator workflows also serve other Symbian devices where their actual ABI and
service contracts work. SDK-provided modern features remain enabled on older
systems unless a real conflict is demonstrated; historical reproduction is not
a capability-selection rule.

The goal is **not** to recreate Nokia's historical development environment verbatim.

The goal is to understand what the historical platform actually did, determine what is genuinely required to produce and execute software on the hardware, and then build the smallest, cleanest, most maintainable modern system that provides those capabilities.

Think of this project as:

> **"What would the Symbian/Nokia 808 development platform look like if it had survived and been redesigned for 2026?"**

rather than:

> "How do we reproduce Nokia's 2010 development environment?"


## Implementation review — 2026-10-01

The initial survey and several vertical slices are complete. Continue from this
checkpoint and [docs/STATUS.md](docs/STATUS.md); do not restart the initial
survey or infer that the platform mission is complete. The original long-term
objectives below remain in force. Current research and replay instructions are
in [docs/RESEARCH.md](docs/RESEARCH.md), [WALKTHROUGH.md](WALKTHROUGH.md),
[docs/BELLE_ABI.md](docs/BELLE_ABI.md) and
[docs/EMULATOR_CONTROL.md](docs/EMULATOR_CONTROL.md).

### What the work established

* Contemporary Clang/LLD, CMake/Ninja and a native E32/SIS implementation can
  replace the normal historical Windows build/package pipeline for the tested
  subset. Original Nokia checksums/whole-image validation and independent
  EKA2L1 loader/CPU/process/installer tests remain valuable behavioral oracles.
* C++20 language features, a named-module example and selected configured
  libc++ headers compile and run in the ROMless probes. The GUI also compiles
  as C++20 and runs with real SDK imports in the guarded firmware experiment.
  These results do not establish a hosted C++20 library or general target ABI.
* The supplied Delight v1.8 archive provides a usable preserved RM-807 ROM/Z
  fixture. Its metadata describes the archive; it does not identify the owner's
  phone, authenticate stock firmware or prove recovery suitability.
* The hardest GUI failures were runtime contracts: a firmware-specific
  executive-call map, a separate ARM TPIDRURO register and SDK cleanup-stack
  setup. No whole-table shift or replacement system-library implementation was
  sufficient evidence. The original emulator profile remains a control.
* Both tested macOS CPU backends now render the counter through real EUSER/WS32,
  accept pointer input, redraw after increment/reset, ignore an outside-control
  tap and exit with guest reason zero and frontend exit zero. Pixel oracles and
  native kernel exit records establish more than a drawing-function breakpoint.
* Live ARM GDB source/ROM breakpoints and stable instruction stepping work.
  Full unwinding, comprehensive crash/process inspection and CLion's debugger
  frontend remain unverified. CLion's GUI source belongs to a separate ARM
  CMake project; its standalone and machine-local presets and clangd context
  now pass checks. Symbian ARM settings are saved for the actual IDEA/CLion
  instance; the live preset uses Default with explicit ARM paths until those
  settings load. The actual IDE configures successfully and resolves all three
  GUI target sources; its remote-debug frontend remains an onboarding gate.

### What needs to change in the execution strategy

Keep the narrow, independently checked experiments, but consolidate them into
repeatable developer workflows before adding more platform breadth. The GUI now
has an owned foreground launcher and generated IDE Run/Remote Debug settings;
source builds, copied fixture, cleanup and post-connection symbol relocation
are checked. This remains a bounded example workflow, not the general lifecycle
manager or symbian test API. Ad hoc
private copies and GDB scripts proved contracts; they are not yet a general
emulator lifecycle API. Seven owned patches currently extend a pinned emulator
checkout. Keep their replay and regression coverage, document ownership and
seek upstreamable changes rather than allowing an unbounded local fork.

Separate four levels of evidence in reports: compiler/format acceptance,
ROMless loader/kernel execution, execution against a named preserved firmware
fixture, and physical-device compatibility. A success at one level must not
silently promote another. Emit fixture hashes and actual scope, including skips
and unresolved warnings. Reproducibility on this host is not a hermetic-build
attestation. Read-only file permissions are not immutable preservation against
the owner; the original archive still needs an independently held offline copy.

The named RM-807 Belle profile now maps the observed 0x10D library-entry-start
and 0x10E load-preparation hooks. A bounded C++ DLL constructor runs through
the process-attach call list; a dynamic client loads, looks up and closes it,
then observes its destructor, on both emulator backends and ARM profiles.
EXE global constructors and destruction also execute after heap setup. SVC
0xFF interception, private executive operations, broader DLL lifetime, TLS, general static
lifetime and a complete hosted C++ runtime remain unresolved. Bounded EXE and
DLL writable data/BSS and typed code/data fixups execute on both backends.
The bounded libc++ container runtime is documented separately in docs/RUNTIME.md. Do not label the experimental toolchain a general Belle
SDK or make these failures disappear by dropping sections or ignoring errors.

### Next work, in priority order

ROM/Z onboarding now uses an independent shared content store and explicit
global -> SDK -> project -> command resolution, documented in
[docs/FIRMWARE.md](docs/FIRMWARE.md). Native EKA2L1 import forms and portable
bundles replace fixed RM-807 paths in production Run/Debug. Generated starters
build and execute on C7, E6, 6120 and E71 using the default executive map;
the RM-807 profile remains restricted to its exact proven pair. EKA1 imports
remain useful, while the missing EKA1 application ABI is reported explicitly.
Continue full import/header/server coverage and general lifecycle acceptance;
this does not complete runtime, concurrency, distribution or physical-device
objectives.

1. **Keep one useful root IDE project and usable generated apps.** The root now exposes the real ARM
   `gui_app` target alongside native tooling, the independently reproducible
   `gui_app_e32` publisher, and a native `gui_app_run` executable that CLion can
   run. Preserve distinct host/guest compile and link rules inside that graph;
   keep the standalone app preset usable. Separate root ARM guest-index
   profiles now build and index all 48 platform-probe C++/C/module/assembly
   sources in the prepared local workspace on both ARM targets with actual
   target compiler commands; the local ARMv6
   IDE profile is enabled alongside host Debug, and the live IDE log confirms
   its CMake configure exited zero. Editor diagnostics/navigation still need
   direct observation. Exercise Run/Stop and the ARM remote
   debugger, including embedded-Python ABI independence and source relocation.
   Keep debugger-profile selection explicit and document the actual loop.
   SDK-owned macOS emulator windows now use a nonbundle child launch and a
   maintained background-window patch; a real desktop GUI test kept the
   browser frontmost. Visually validate that a new display stays on the
   desktop while another app is full-screen, without switching the owner's
   active Space, before closing the focus/placement acceptance gate.
   `symbian init` now registers the CMake workspace/C++ module and generates
   an enabled stable IDE profile (independent of preset profile-ID changes),
   linked visible SDK
   headers/runtime, an initial build and actual host Run/Debug launchers.
   `symbian sdk install` materializes target inputs and tooling; project CLI
   calls dispatch to its local SDK selection. Keep CMake as the sole include/ABI
   authority, relative shared project files and one ignored SDK location.
   The local SDK location is a switchable path, not a project-level digest
   lock. CLI and direct CMake builds now reconfigure when it changes; detailed
   source-input provenance belongs under `research/`, outside applications.
   The actual repaired app-3 IDE import resolves three sources without unknowns;
   header analysis and saved-launcher execution are separate maintained checks.
   Complete live IDE debugger validation and full host payload closure before
   claiming a clean-machine one-install experience.
   Treat static archives and loadable DLLs as ordinary project outputs too:
   the same target architecture, SDK imports, reproducible build records,
   source-level symbols and debugger model should apply. Static libraries need
   link/use and source-breakpoint tests in an app; DLLs need frozen exports,
   import/relocation checks, per-process writable storage, entry/exit lifetime
   and module symbol loading tests before a general-purpose claim. A validated
   E32 DLL header by itself is only a format checkpoint. Generated library
   projects should avoid historical MMP details and report missing runtime or
   loader capability at the build/load boundary.
   First bounded DLL progress: the installed SDK now has a C compiler and CMake
   E32 DLL/proxy publisher. ARMv5T/ARMv6 C DLLs and a consumer import build;
   a writable-state DLL resets per process on Dyncom/Dynarmic. A SHA-256 subset
   from the Mbed TLS C archive links, converts and executes through a dynamic
   DLL client on Dynarmic/Dyncom, with a changed-input digest control.
   A bounded SDK C++ DLL constructor now executes on process attach on both
   target architectures and emulator backends, with a changed-constructor
   negative control. A real dynamic client also verifies load, two ordinal
   lookups, close/destructor and an absent-DLL error on both CPU backends and
   ARM profiles. General hidden/internal writable data, TLS, repeated unload,
   module symbols and complete debugger/lifetime behavior remain gates.
2. **Build a real guest C++ runtime as a required SDK component.** The first
   maintained subset now compiles original LLVM libc++ string/new-helper sources
   and executes heap-backed std::string/std::vector on both emulator CPU backends.
   Its independent arithmetic, repeated destruction/heap-count, nothrow failure,
   fatal allocation and original-validator controls pass. Bounded EXE writable
   data/BSS, code/data pointer fixups and zero-fill now pass both-backend execution
   and independent original validation. Bounded EXE global initialization and
   destruction now pass four named-firmware cases. Next support DLL detach and
   unload beyond the bounded `RLibrary` close/destructor case, local-static
   guards, broader writable DLL lifetime and TLS,
   compiler-rt and required C services. The SDK now supplies a Clang-compatible
   OpenC varargs header and original libc++ error categories. Real `libc.dll`
   `vsnprintf` and `std::error_code`/`strerror_r` pass source-tree normal and
   changed-result controls on both ARM targets and emulator CPU backends;
   fresh-SDK installed controls also pass. A maintained, separate classic-C-locale
   and `ostringstream` profile now executes normal and changed-result controls
   on ARMv5T/ARMv6 and both emulator backends from source and a sealed SDK
   export (eight cases in each matrix). It is `Symbian::Streams`, an alternate
   runtime archive, not an add-on to the default archive. Only C/POSIX locale
   is supported; invalid locale requests return `EINVAL` at the adapter API.
   A source-built ARMv5T/ARMv6 × Dyncom/Dynarmic matrix executes original
   compiler-rt soft-double operations, with a replayable ARMv5-safe patch.
   A bounded original-SDK `RChunk` create/grow/close
   probe now passes all ARMv5T/ARMv6 × backend normal/negative controls, establishing
   a possible native page source rather than a finished Abseil allocator. A
   subsequent SDK-owned bridge now creates exact page-multiple, page-aligned
   process-owned chunks and releases the owner plus metadata with a balanced
   same-thread heap-cell control. Worker `pthread` key isolation and its
   thread-exit destructor execute; EUSER ordered byte exchange and selected
   OpenC parsing/copy/page-size services also execute. A replayable Abseil
   guard uses its existing `GetTID()` fallback to remove one unsupported ELF
   TLS relocation from the isolated link. General ELF TLS, cross-thread page
   ownership and wider synchronization remain gates beyond the tested subset.
   The pinned A11
   `LowLevelAlloc` with an SDK-owned `RChunk` page backend now executes a
   same-thread 130,000-byte allocation and explicit arena lifetime control on
   RM-807/Dynarmic, with a changed-result control. This is a tested allocator
   slice. The verified SDK export packages the pinned Abseil headers and 43
   Status/StatusOr/flat-hash-map closure archives per target;
   eight source and eight installed-target firmware-backed controls pass on
   ARMv5T/ARMv6 × Dyncom/Dynarmic. Continue guest time, broader Abseil APIs,
   allocator/thread ownership and A11 concurrency; do not generalize the
   bounded execution result to all Abseil components. The
   converter now supports validated imported **function** pointers through
   PLT and E32 data relocation, with ARMv5T/ARMv6 × two-backend execution
   and malformed-input controls. Imported **data objects** remain a separate
   gate for typed EH and broader ABI support. LLD's exact ARM-to-Thumb long
   thunk form now receives a validated E32 relocation and executes on both
   ARM profiles/backends; other linker-generated absolute forms need audits.
   Bounded local GOT
   relocation now runs
   global std::nothrow, constant-object reads and a Thumb function on both
   firmware-backed CPU backends, with changed-value and malformed-image controls.
   Generated apps use fallible model acquisition and return -4 after native
   resource cleanup on failure. External data-object imports/general GOT
   remain gates.
   Over-aligned allocation, original ARM 32-/64-bit division/remainder,
   aligned memcpy and soft-float arithmetic/conversion builtins now have
   maintained execution probes. Original libc++ `hash.cpp` and a real
   `std::unordered_set` growth/cleanup path execute on both ARM targets and
   emulator backends; its `ceilf` import remains ROM-dependent. Continue
   broader compiler-rt and
  new-handler policy, and port guest Abseil
   Status/StatusOr, flat_hash_map and nlohmann::json. Each capability needs
   failing controls and cleanup checks. Evaluate mimalloc or another fast
   allocator as an optional SDK-owned backend only after verifying Symbian page
   sourcing, thread-local heap startup, cross-thread free, DLL lifetime,
   bounded memory use and the current ordinary versus nothrow allocation
   failure contract. Compare latency, fragmentation, peak committed memory
   and UI latency with the current `User::Alloc` adapter on both CPU backends
   before changing the default. Deliver the A11 concurrency gates below:
   native completion integration and stackless tasks first, then fibers pinned
   to one OS thread, then optional bounded workers. This is a required SDK
   workstream with its own runtime prerequisites and performance acceptance,
   rather than a deferred instruction to reuse a thread library. A bounded
   container probe is not all hosted C++20.
   Keep CXX_CAVEATS.md focused on developer-facing OS contracts and remaining
   manual workarounds; remove workarounds when verified SDK adapters replace them.
3. **Turn the one-pip-install design into distributable payloads.** Follow
   docs/DISTRIBUTION.md: host extension plus versioned platform payload wheels
   for materialized headers, native compiler/linker, emulator and debugger.
   Preserve licenses/provenance, offline operation and relocatable closure;
   firmware remains a separately supplied local input. Resolve canonical
   pybind11_abseil ownership/co-installation with A11 before public distribution.
   Linux aarch64 host native/wheel/audit checks now pass; x86_64/other CPython,
   Linux emulator/guest/debugger and clean-host end-to-end installs remain gates.
4. **Consolidate the emulator lifecycle and `symbian test`.** Build on native
   capture/input/exit endpoints and installer checks. Add explicitly owned
   create/start/install/launch/log/stop/reset operations with typed serializable
   manifests, digests, bounded process lifetime and retained artifacts. Stopped
   copies are the initial snapshot mechanism, not live-machine snapshots.
   Acceptance: two independent fresh GUI runs, startup/failure/timeout/stale
   endpoint controls and unchanged golden inputs. Diagnose intermittent frontend
   shutdown with retained native stacks; repeated success does not explain it.
5. **Close the demonstrated firmware ABI gaps.** Keep the observed 0x10D
   library-entry-start mapping profile guarded; trace 0xFF and remaining private
   operations using original source, actual callers and live observations.
   Test DLL detach/cleanup,
   focus/occlusion/orientation and resource lifetime. Keep exact-ROM opt-in guards
   and the default-profile control; new firmware needs independent evidence.
6. **Make debugging failures useful.** Add process/thread/module inspection,
   panic capture, symbolication and a demonstrated unwind contract. Test the
   actual CLion debugger frontend; terminal GDB success is a separate gate.
7. **Expand system and device scope only after these gates pass.** Add resources
   and registration, then a minimal server/DLL/IPC slice. Complete preservation
   and a device broker before physical deployment. Recovery, erasure, calibration,
   partition/bootloader operations retain their human/device boundaries.

### Preserve useful runtime properties while improving the implementation

Modern interfaces and new low-level support are encouraged. Preserve an OS
contract when it remains necessary; preserve an implementation only when its
measured properties justify it. A familiar replacement must not silently add
blocking, copying, synchronization, memory commitment or power consumption.

| Property to preserve | Concern with a naive replacement | SDK direction |
| --- | --- | --- |
| Native asynchronous completion | One blocking worker per request adds stacks, context switches and wakeups; independent wait loops can consume each other's thread request signals. | One completion owner per event thread, with an A11 backend translating native requests into continuations and cooperative fiber wakeups. |
| Serialized event handling | Moving handlers to a generic pool introduces races and violates thread/handle affinity; long handlers or an endless inline pump starve UI events. | Explicit event-thread affinity, bounded turns and deliberate computation offload. Test nested-loop reentrancy as well as parallel races. |
| Thread-relative heaps and handles | Destruction through the receiving thread's current heap may free through the wrong allocator; not all sessions/handles can be used on arbitrary workers. | Track allocator identity and release ownership, verify session sharing, and initially prohibit fiber migration. Do not solve every allocation with one global lock. |
| Borrowed, length-aware descriptors | Owning copies and repeated UTF-8/UTF-16 conversion can allocate on every event; a temporary adapter can expire during an asynchronous request. | Modern strings/views internally, reusable conversion storage and descriptor adapters at native calls, with lifetimes extending through completion. |
| Batched native services | Flushing or synchronously querying after each drawing operation creates avoidable server round trips. | Preserve Window Server buffering, batch work, and make flush/synchronization boundaries explicit. |
| Predictable failure and cleanup | Ordinary RAII cannot be assumed to unwind across native leaves through exceptions-disabled frames; an allocation-heavy error path can fail while reporting OOM. | Verified leave-to-Status boundaries, bounded failure paths and cancellation that drains native requests before releasing resources. |
| Efficient target code | A library may pull in large dependencies, locked atomics, software arithmetic or costly generic operations on the actual target. | Implement missing compiler/runtime services correctly, inspect generated code and measure their cost. Do not substitute incorrect stubs to shrink a binary. |

The SDK should absorb these concerns wherever possible. Application developers
should mainly choose ownership, affinity and asynchronous composition; they
should not manually reproduce heap/TLS startup or native cancellation plumbing.
ARMv6 and ARMv5T guest runtime profiles now execute on both emulator CPU
backends. The ARMv6 default applies to new projects, while legacy projects
retain their ARMv5T choice. Compiler attributes and E32 CPU metadata must
agree before publication or launch. This is a prerequisite for A11 atomic and
context-switch work, not evidence that either concurrency backend runs yet.
Keep remaining requirements visible in [CXX_CAVEATS.md](CXX_CAVEATS.md).

Establish comparable raw-native, A11 stackless and A11 fiber benchmarks for
timer/event dispatch, request cancellation, rendering bursts and bounded compute.
Record allocations per operation, peak and committed memory, fiber stack high
water marks, context switches, idle wakeups, server calls, throughput and tail
UI latency. Include encoding costs and failure/shutdown paths. Derive budgets
from workloads and raw-native baselines before accepting replacements; no
unmeasured claim that either historical code or modern C++ is automatically
faster. Use physical-phone runs for performance/energy conclusions only after
the existing device gates are satisfied. Emulator results establish functional
behaviour and diagnostic baselines, not phone battery or CPU performance.

### Explicit A11 concurrency delivery path

**Implementation reference inspected on 2026-10-01:** local A11 HEAD
`fcccb8cb6e1e67d7ba0822ac14cece9ee4c7091b`, including actual working-tree files
under `cpp/thread/`, `cpp/a11/concurrency/` and `cpp/python/interop.*`. Record
source digests as well as the commit when importing; HEAD alone does not pin
uncommitted source. Preserve upstream licenses and maintain explicit adaptation
patches. The following is a delivery plan, not a claim of a working guest port.

Reuse A11's actual APIs and semantics rather than implementing a lookalike:

* `a11::Promise<T>` / `Future<T>` carry `absl::StatusOr<T>`; `Task` is
  `Future<Unit>`, not inherently a coroutine or a scheduled fiber. `OnReady`
  provides the non-blocking path and may run inline. Preserve that behaviour;
  marshal an affinity-sensitive continuation explicitly to its owning executor.
* `thread::Post` / `PostAt` run stackless callbacks, but currently target A11's
  shared worker pool. `a11::Submit`, `SubmitTask`, `Schedule` and
  `ThenAfterWaiting`'s pending path use fibers. Importing their headers does not
  provide a stackless or single-thread implementation of those operations.
* `DriveInline` has reentry-depth bookkeeping, and `CallbackScheduler` has
  bounded callback turns. Its current implementation requires at least two
  turns in flight; setting its concurrency to one is not a valid port. Audit
  the progress dependency and provide a tested A11 backend adaptation for a
  serialized event executor. Depth limits alone do not guarantee UI fairness.
* A11's current fiber implementation uses Boost.Fiber/Context, native parking
  primitives, TLS and worker scheduling. Its build enables exceptions for
  `boost_primitives.cc` and `thread_pool.cc`, including Boost teardown's
  forced-unwind contract. The SDK now follows A11's selected-translation-unit
  exception policy: exceptions stay off by default, but the host Boost
  boundary may enable them explicitly. The pinned full host source builds and
  passes its original tests in isolation; replacing the staged SDK archive and
  closing its distribution dependencies remain work. Do not suppress forced
  unwind. The Symbian guest backend still needs a verified ARM-specific
  adaptation of structured cancellation, joining, reaping and destruction.

The initial architecture is one A11 event executor attached to the application's
existing native completion loop. Stackless continuations and, later, pinned
fibers share its runnable-work policy. The OS thread sleeps only when there is
no runnable work, on native request completion and the next required timer.
When a framework owns `CActiveScheduler`, integrate with that scheduler; when
the application owns its wait loop, provide the equivalent completion bridge.
Neither case adds an independent request-semaphore consumer or a competing
desktop-style event loop. A fiber run queue is necessary internal scheduling,
not permission to introduce a second unrelated concurrency framework.

**C0 — Pin the code and establish runtime prerequisites.**

The exact 46-file A11 source snapshot and 92 local include edges are now staged
under `cpp/symbian/concurrency/upstream`, with digests, original paths/license,
actual tests and an integrity-check target. The unmodified snapshot is not
itself a guest backend. Bounded writable EXE data/BSS is verified;
the remaining prerequisite gates below are still open. See
`cpp/symbian/concurrency/README.md`.

A bounded shared host/guest stackless adaptation now executes A11's completion semantics:
shared Promise/Future/Task, inline OnReady/Then, cancellation requests,
abandonment, ordered nonblocking JoinAll, bounded reentrant DriveInline and
TaskGroup's explicit asynchronous join/cancellation.
It uses `thread::Mutex` over the verified libc++ OS-thread lock, with a
subsequently added cooperative fiber park path. Eight normal and
changed-result cases pass across ARMv5T/ARMv6 and Dyncom/Dynarmic, both from
the source tree and from a fresh exported SDK through `Symbian::Stackless`.
This was the C2 subset before the separate bounded C3 fiber archive; it is
still not a full A11 public API or worker scheduler. The guest adaptation carries actual
`absl::StatusOr<T>` and accepts `absl::Status` directly. Its installed-SDK
normal/changed stackless and timer controls each pass 8/8 on ARMv5T/ARMv6 and
Dyncom/Dynarmic. Pinned guest Abseil Status/StatusOr, Cord payloads,
flat_hash_map and selected time functions execute; general guest Abseil and
TLS remain open. One native timer request owner and event-thread pump pass C1/C2
integration controls, but general request ownership and scheduler integration
remain C1/C2 gates.
The portable channel, Future, TaskGroup, mailbox and inline-pump layer now has
one source tree under `cpp/symbian/concurrency/common/`. The host
`symbian::concurrency` CMake target selects an opaque Boost-free primitive ABI
backed by one bundled static archive containing Boost.Fiber/Context objects;
ordinary host consumers do not include or link separate Boost libraries. The
host build requires Boost only when rebuilding this archive. Guest builds use
the ARM-aware backend and export
the same common headers as `<thread/channel.h>` and
`<symbian/concurrency/*.h>`. `thread::Mutex`, `MutexLock`, `CondVar` and
`Channel<T>` retain their A11 names, while bounded Future/Task stays under
`symbian::concurrency`; no incompatible header occupies `a11::`.
The host additionally has a bounded `thread::Fiber` with explicit same-thread
join, cooperative cancellation and normal C++ cleanup. A11's park guard and
custom scheduler policy now protect idle fiber waits, with the CPython boundary
dropping and restoring the GIL there. Host `Post`/`PostAt` use a shared
stackless callback pool and A11's original MPMC queue; A11's original
`PermanentEvent`/`Select` protocol also executes. An asyncio Future bridge
and deferred Python-reference holder run from the installed macOS wheel. This
host adaptation does not yet claim A11 fiber trees, worker context stealing,
Submit/Schedule, selectable channels, introspection or full shutdown parity.
The guest now has a separate `thread::Scheduler`/`Fiber` archive above the
verified ARM switch. Its replaceable `SchedulerPolicy` chooses ready-fiber
order and receives wake notifications outside internal locks. Guest
`thread::Mutex`/`CondVar` park fibers cooperatively, and unresolved
`Future::Await` parks only inside a fiber. This bounded C3 slice does not yet
have A11's shared pool, fiber tree, `Select`, native request integration,
cancellation or join contract.

Inventory the actual A11 dependency closure and separate its platform backend
from portable completion, cancellation, channel/select and pump logic. Put owned
native adapters in `cpp/symbian/concurrency/`; preserve the `thread::` and
`a11::` interfaces and package the actual imported sources/headers. Add explicit
CMake components so stackless applications do not link the fiber/worker backend
or diagnostic dependencies they do not use. This build separation is work to
implement, not an existing A11 feature.

The original EUSER `RFastLock` now passes a bounded guest create/poll/held
timeout/wait/signal/close probe on both client targets and CPU backends. Use
that fast native primitive for OS-thread synchronization where verified, but
keep A11's `thread::Mutex` fiber-aware: contention on the event OS thread must
park a fiber through A11's backend, not block the OS thread. An ARM AAPCS
context switch may be fast, but must preserve registers/alignment and remain
within one verified thread/TLS/heap/leave-state domain.

Clang's observed ARMv5T `__atomic_*` and ARMv6 `__sync_*` 32-bit libcalls now
route through the original EUSER atomic operations. A single-thread
fetch-add/load/CAS/exchange probe and changed-result control pass on both
architectures and CPU backends, including the standard installed SDK proxy.
The same eight cases also execute when linked against the installed SDK's
prebuilt runtime archive and headers.
A bounded parent/secondary `RThread` probe now passes 4,000 shared atomic
increments plus a changed-result control on both ARM profiles and emulator
backends. It uses original `RThread` create/logon/resume/join/exit imports, a
separate worker heap and an observed Belle 0x34 exit-reason mapping. This is
not an A11 executor or general thread service. The threaded libc++ profile now
passes a separate bounded `std::thread`/shared and unique ownership race probe,
including worker destruction, on both ARM profiles and emulator CPU backends;
the 16 normal/negative cases also pass against a fresh installed-SDK export.
The 64-bit compiler ABI now has a lock-backed default and an opt-in ROM-native
archive. On the verified RM-807 fixture, the native path reaches LDREXD/
STREXD and passes parent/worker normal and changed-result controls on both
emulator CPU backends after the Dyncom STREXD correction. A replayable libc++
adaptation makes `atomic` and `atomic_ref` lock-free queries reflect the
selected runtime instead of Clang's ARMv6 builtin assumption. Other firmware,
wider race patterns, complete atomic ABI and ordering coverage, and heap/TLS
shutdown remain C0 gates. Record performance hypotheses and
future measurements in [PERFORMANCE_CONSIDERATIONS.md](PERFORMANCE_CONSIDERATIONS.md).

The guest runtime must support C++ exceptions as an explicit application
profile while retaining `-fno-exceptions` as the SDK default. This is distinct
from A11's selected-exception host Boost boundary. Before enabling the profile,
preserve `.ARM.exidx`/`.ARM.extab`,
emit and validate the Symbian exception descriptor and E32 header offset,
resolve the pinned `drtaeabi`/libc++ ABI closure, and execute throw/catch,
destructor-unwind and failure controls on both ARM targets and CPU backends.
An isolated probe now preserves and converts the unwind index, extension table
and four-word descriptor, and executes a no-throw landing-pad/cleanup path with
negative controls on both ARM targets and emulator backends. The standard SDK
linker profile still discards unwind sections. Typed throw needs an imported
type-info object (`R_ARM_GLOB_DAT`), which the current function-only ordinal
resolver explicitly rejects. Implement and validate imported data, then test
real throw/catch/unwind before enabling the opt-in profile.
`std::promise`/`std::future` currently fail to link on exception-pointer and
error-category symbols. An exceptions flag alone is not a capability claim.

Gate guest Abseil Status/StatusOr, invocables, clock/time and required STL support,
atomics, shared ownership, static lifetime and TLS against execution tests.
The first guest clock slice now executes original libc++ `steady_clock` and
`system_clock` on named firmware. A narrow Symbian `steady_clock` adapter uses
the nanokernel tick plus its HAL period, with an ordinary-tick fallback;
RM-807's OpenC `CLOCK_MONOTONIC` returned `EINVAL`. EKA2L1 now reports the
tick/frequency HAL values and emits FastCounter ticks at its advertised rate.
Two-thread clock samples and ordered handoffs now pass on both ARM targets and
emulator backends. The half-wrap and suspend/long-idle behavior still needs
broader controls before native monotonic deadlines in C1 can rely on it.
Inspect logging/diagnostic dependencies rather than linking the entire desktop
closure blindly. Provide platform implementations or reject unavailable
capabilities explicitly; do not replace synchronization with no-op locks under
an assumed single-thread model. Run portable semantic GTests on macOS/Linux,
then native guest probes on both emulator CPU backends. Compilation alone does
not satisfy this gate.

**C1 — Own native requests, deadlines and wakeups.**

The first bounded `RTimer` request owner now executes on both ARM targets and
emulator backends, including overlap rejection, cancel/close drainage and
allocation balance. The shared Window Server pump and deadline/wakeup
semantics below remain the C1 gate.

The first `TimerPump` bridge now translates timer completion into A11-derived
Tasks, accepts `absl::Time` absolute deadlines and `absl::Duration` relative
delays, and forwards worker cancellation through a
coalesced event-thread request signal. Source and installed guest controls
pass. Ordinary new `symbian init` projects now combine it with Window Server
event/redraw statuses through one request-semaphore wait when their selected
firmware supplies `libpthread.dll`. The original counter example also uses a
timer Future for a delayed marker and cancels it on Reset; both apps run on
both emulator CPU backends. Explicit `init --firmware` selects the portable
profile on Z drives without that DLL. These application migrations do not
establish A11 fibers or `thread::` compatibility;
the pump now limits admitted timer requests (64 by default), returning an
already-ready resource-exhausted Task at capacity. Broaden race/error controls
and native I/O registration before C1/C2 acceptance.
The accepted absolute deadline is converted once from real-world wall time to
monotonic relative waiting. Long waits use signed-32-bit-microsecond native
arms; a simulated wall-clock jump after registration, infinity, range limits
and 24-hour cancellation passed guest controls. Actual clock changes and a
full-length rearm remain separate acceptance cases.

A second owned native request now uses `RProperty::Subscribe` and returns a
typed `Future<int>`. It owns its status, integer result, property handle and
definition, registers ownership before submit, handles immediate completion,
drains cancellation before close and publishes after removing its request.
The timer and property owners share one event-thread request-semaphore wait.
Normal/changed controls passed 8/8 on both architectures and CPU backends.

Implement a native adapter owning each request status, buffer, cancellation
operation and completion registration. Start with `RTimer` and Window Server
events/redraws. Register ownership before submitting work, handle immediate
completion, and publish each final result once. Cancellation is a request;
retain resources until the API's actual completion/drain contract is fulfilled.
Use an explicit monotonic deadline model and a verified native timer bridge;
changing home time must not corrupt scheduler deadlines. Coalesce wakeups and
park while idle instead of polling.

Acceptance includes completion before waiting, multiple simultaneous requests,
cancellation versus completion, timer expiry, close during pending I/O, native
errors, stale registrations and reentrant callbacks. Tests must prove no lost
wakeups, double completion or use after free, and no unsolicited extra OS thread.

**C2 — Deliver useful stackless tasks/promises/pumps first.**

Port the real Promise/Future/Task completion path and required A11 synchronization
backend, with native completions driving promises directly. Port `OnReady`/`Then`
composition and pumps with explicit affinity, bounded queue growth, turn/time
budgets and preserved immediate-completion semantics. Provide a tested backend
route for `Post`/`PostAt` and explicit event-executor binding; do not globally
redirect computation work to the UI thread without a documented contract.

An unresolved `Await` on the event thread outside a supported fiber must fail
explicitly rather than blocking the thread that must deliver its completion.
Ready-result access remains valid. Do not implement blocking awaits by entering
unbounded nested event loops, or describe `SubmitTask` as stackless until its
actual execution route is supported. Test promise abandonment, cancellation
hooks, observer/result lifetimes, synchronous callback reentry, scheduler lifetime
and progress when one pump is suspended or yields to another.

Acceptance: a GUI remains interactive while timers and service requests overlap,
without a fiber stack or an extra worker per outstanding operation. Compare its
allocations, idle behaviour and dispatch latency with the raw-native baseline.
Expose this capability in the SDK and project templates once it is verified;
do not wait for the complete fiber backend to ship useful async composition.

An `EventMailbox` now admits bounded cross-thread UI dispatch, executes no more
than a requested turn budget outside its lock and uses the same coalesced
wakeup. Its name deliberately differs from A11 `Post`, which still belongs to
the shared pool. A ready Future may be read through `Await`; an unresolved
`Await` fails with `FailedPrecondition` until a verified fiber backend exists.

The A11-derived `TaskGroup` now has a guest execution control over real native
`TimerPump` requests: aggregate success waits for both completions, and
aggregate cancellation forwards to three child timers that the event thread
drains before publishing the result. This validates one C2/C1 integration
slice; property change subscriptions and bounded event dispatch add another
slice. Arbitrary service requests, full A11 scheduler and fibers remain
separate gates. Guest `thread::Mutex`, `MutexLock`, `CondVar` and `Channel<T>`
now have a bounded fiber-aware backend: a running fiber parks on contention,
signal or timeout, while ordinary OS-thread callers still use native blocking
waits. `EventMailbox` uses nonblocking channel operations. The guest's
`thread::SleepFor` parks a fiber; outside a fiber it blocks an OS worker and
must not be used on the event thread. A11 fiber-aware `Select` and
`PermanentEvent` remain C3 gates, as does native request pump integration.

**C3 — Add stackful A11 fibers pinned to one actual OS thread.**

The first C3 prerequisite is now in the SDK runtime archive: a no-throw ARM
`SymbianFiberSwap` preserves AAPCS callee-saved registers and switches a
bounded 16 KiB heap stack on one OS thread. A retained Thumb/C++ probe pauses
with heap-backed `std::string` and `std::unique_ptr` live, resumes, destroys
them normally and balances heap cells. Normal/changed controls pass 8/8 on
ARMv5T/ARMv6 × Dyncom/Dynarmic from a sealed installed SDK candidate. A
subsequent bounded scheduler and fiber-aware mutex/condition-variable slice
now executes custom ready ordering, cross-thread wake, timed wait and Future
await. Stack overflow protection, floating-point context, native TRAP safety,
debugger-visible waits and the full A11 `thread::` feature set remain open.

Implement and verify the ARM/Thumb context-switch and stack-allocation backend,
including ABI-required registers, alignment, any relevant floating-point state,
stack overflow detection and debugger-visible fiber identity. Fibers yield to
the same event executor while waiting, so native requests continue to complete.
Implement fiber-aware mutexes, events, channels/select, sleep, cancellation trees,
join and reaping with A11's existing semantics. A synchronous native call still
blocks the entire OS thread; a fiber does not turn it into asynchronous I/O.

Start with fixed bounded stacks and an explicit memory budget. A11 currently
defaults to 512 KiB based on its desktop workloads and describes demand-backed
stack allocation. Neither that size nor its resident-memory assumptions are
automatically suitable for Symbian chunks/heaps. Measure reservation, commitment,
high water and deep inline drives; do not claim segmented/growing stacks before
implementing their compiler/OS support. Reject unavailable stack modes clearly.

Thread TLS, current heap and cleanup/trap state are shared by fibers on one OS
thread; changing SP does not make them fiber-local. Initially forbid switching
across an active native leave/TRAP boundary or other unverified thread-state
scope. Establish a safe adapter boundary before permitting native leaving calls.
Do not invent per-fiber OS TLS or silently switch heaps. Context/cleanup-state
virtualization may be implemented later with source and execution evidence.
Audit the framework's own dispatch/TRAP frames too: if every callback runs under
a native trap, simply switching stacks inside `RunL` does not satisfy this rule.
Either establish a verified dispatch boundary or implement the necessary state
handling before enabling fibers in that framework; stackless tasks can ship first.

Acceptance: independent fibers retain their locals across repeated switches;
native timers and UI requests wake the right waiters; cancellation runs normal
C++ cleanup and joins/reaps without leaked stacks or pending requests; exhausted
stack/heap and shutdown paths are observable. Test starvation and nested pump
depth. Extend GDB diagnostics to distinguish fiber stacks from the owning OS
thread and report suspended waits. Fibers remain pinned throughout this stage.

**C4 — Add optional bounded OS workers through the same A11 backend.**

Only after secondary-thread heap/TLS/startup/exit and synchronization contracts
pass, add explicit computation offload with bounded workers and queues. Workers
return results to the owning event executor. Default native handles and pending
requests remain on their owner thread; sharing or duplication needs API-specific
proof. Allocations crossing threads must retain the allocator that frees them.
Audit cross-thread promise completion and reference destruction before enabling
them. Keep fiber migration/work stealing disabled until allocator, TLS, affinity
and native-request ownership all permit it; one-thread fibers remain a supported
configuration even after workers exist.

**C5 — Ship and observe the complete integration.**

Export tested concurrency capability profiles through the installed SDK/CMake
package and project wizard, with usable examples for stackless service work,
pinned-fiber awaits and optional worker computation. Keep ARM semantic tests and
negative controls separate from host GTests/Pytests and physical performance
results. Record imported A11 revision/digests, backend changes, skipped features
and benchmark budgets in release manifests. Linux host support follows the same
component/build/test contracts rather than a separate scheduler implementation.

Host Python tooling is a distinct integration: copy A11's actual Future/asyncio
bridge, event-loop capture/resolution, scheduler park guard and deferred Python
reference holders when asynchronous native host work is introduced. Release the
GIL for native work, acquire it for Python access, and test loop closure,
cancellation, interpreter shutdown and destruction on a worker.
`SchedulerParkGuard` must release and restore the host lock on the same thread,
including timeout wakes; install it before fibers run. Copy `DeferredPythonRefs`
retirement/draining: destructors must not acquire the GIL, and finalization
checks alone do not make acquiring it safe. Drain only while already holding
the GIL, with the inherited shutdown policy explicitly tested. This does not
imply Python runs on the phone or require Python machinery in guest concurrency.

Open implementation questions are explicit gates: the smallest genuine A11
component split; serialized callback-pump progress; guest fiber teardown under
the default no-exceptions profile;
safe interaction with native leave/cleanup state; Symbian stack commitment and
guard support; monotonic timer precision; and allocator ownership across workers.
Resolve them with maintained probes and record results in RESEARCH_LOG/STATUS.

### Implementation constraints

Use A11 as the practical implementation reference: Python policy in `symbian/`,
native libraries in `cpp/symbian/<component>/`, bindings in `cpp/python/`, Google
style/docstrings, exceptions disabled with Abseil Status/StatusOr, and exceptions
only at pybind11 translation boundaries. Native format logic must remain native.
Use A11's thread library if adding native concurrency; existing emulator event
loops may be reused without introducing another scheduler. Async Python bindings
must follow A11's GIL and deferred reference-holder patterns. Synchronous parsers
and the socket policy client need no new callback/holder infrastructure.

Python package initializers expose shortcuts only; implementation belongs in
named modules. Use Pydantic BaseModel for serializable metadata. The actual A11
Status/StatusOr bridge is ported, including its canonical caster/runtime modules.
Use standard C++/Abseil/nlohmann types internally; Qt types belong at external
API calls. Ordered format tables remain ordered for deterministic serialization.
The placement-new conflict between preserved SDK headers and libc++ is real;
isolate the SDK translation unit behind a small C ABI rather than rewriting
platform headers or exchanging modern STL objects with frozen DLL interfaces.

Use GTest/Pytest, Black/Ruff at 80 columns, .clang-format, CMake presets/Ninja and
actual compile_commands.json. Keep upstream checkouts, firmware/private data,
builds and runtime artifacts ignored; preserve licenses. The GPL control adapter
belongs only to the research emulator, not the production Python extension.
Keep test evidence/counts in STATUS/RESEARCH_LOG current rather than treating
previous checkpoint numbers as a
permanent acceptance threshold.

---

# 1. Core principles

These principles take precedence over convenience and should guide architectural decisions throughout the project.

## 1.1 Nothing in the historical platform is sacred

Do **not** assume that an existing Nokia/Symbian tool, compiler, build system, runtime, SDK component, file format implementation, emulator facility, compatibility layer, or historical dependency must be preserved merely because Nokia used it.

For every historical component, ask:

1. What problem did this component solve?
2. Why was it implemented that way?
3. What input/output or ABI contract does it actually provide?
4. Is that contract still necessary?
5. Can the underlying functionality be implemented more simply today?
6. Can a modern implementation eliminate an entire historical dependency?
7. Can the functionality be implemented natively on macOS?
8. Can the functionality be made substantially more observable, testable, deterministic, or maintainable?
9. Which ownership, scheduling, batching or bounded-memory properties did it
   preserve, and what measured evidence shows the replacement preserves or
   improves them?

Prefer understanding and rebuilding over blindly preserving.

A small amount of compatibility code around a genuinely necessary historical interface is acceptable.

A large historical runtime stack that exists merely because "that is how Symbian development worked" is not.

---

## 1.2 Minimise compatibility layers

Actively minimise:

* historical runtimes
* old interpreters
* compatibility shells
* Wine/Windows dependencies
* obsolete build systems
* ancient compiler toolchains
* duplicated SDK infrastructure
* historical GUI development environments
* proprietary utilities
* opaque binary tools
* unnecessary emulation

If a compatibility layer is proposed, document **why it is genuinely necessary**.

Do not introduce a compatibility layer merely because it is easier than understanding the underlying problem.

A native reimplementation is preferable when the functionality is sufficiently understood and the implementation is tractable.

---

## 1.3 The historical implementation is evidence, not architecture

Historical Symbian SDKs, binaries, source code, documentation, firmware and tools are valuable sources of information.

They are not automatically the architecture of the new system.

Use them as:

* specifications
* compatibility references
* behavioural oracles
* reverse-engineering targets
* sources of ABI information
* sources of historical knowledge

not as things that must necessarily be carried forward.

---

## 1.4 Prefer modern interfaces

The user is developing on macOS in 2026.

Normal workflows should therefore feel like modern software development:

* Git
* CMake where appropriate
* Ninja
* Clang/LLVM
* clangd
* LLDB/GDB where useful
* modern Python
* modern scripting
* reproducible builds
* deterministic tests
* structured logs
* symbolication
* source indexing
* automated emulator execution
* machine-readable diagnostics
* coding-agent integration

Do not make developers interact directly with historical tools unless there is a compelling technical reason.

---

# 2. The target platform

The initial target is specifically:

**Nokia 808 PureView, RM-807, Symbian Belle / Belle FP2-era software.**

The platform should ultimately support:

* ordinary Symbian applications
* Qt/S60 applications where useful
* native Symbian C++ applications
* system servers and DLLs
* plugins
* system components
* IPC-heavy components
* lower-level experimentation
* reverse engineering
* debugging
* potentially custom/alternative OS development

Do not assume all of these need to be supported simultaneously.

Build the infrastructure progressively.

---

# 3. macOS must be the canonical development environment

The normal development loop must run directly on macOS.

The project should not require a Windows VM for:

* editing
* normal compilation
* static analysis
* source navigation
* packaging
* emulator execution
* automated tests
* debugging
* reverse engineering
* ordinary application development

A developer should eventually be able to do things resembling:

```bash
symbian doctor
symbian build
symbian test
symbian package
symbian emu
symbian emu install foo.sis
symbian emu launch foo
symbian emu screenshot
symbian emu logs
symbian emu debug foo
```

without leaving macOS.

---

# 4. VMs are recovery tools, not development infrastructure

There should be **no general Windows VM dependency in the normal development stack**.

A VM may be retained for one narrowly defined purpose:

## Historical recovery

Maintain a sealed, reproducible VM containing whatever original Nokia/Symbian tools are genuinely required for operations that cannot reasonably be replaced.

Examples may include:

* Phoenix
* historical Nokia USB drivers
* historical firmware flashing utilities
* genuinely unreproducible proprietary tooling
* unusual device-recovery operations

The recovery VM should be treated as an appliance.

It should not become the place where ordinary development happens.

Do not add arbitrary development tools to it.

Do not make the normal build system depend on it.

Do not expose it as a general-purpose coding-agent environment.

---

# 5. Preserve the physical Nokia 808

Treat the physical phone as valuable hardware and assume unfinished software can damage it.

Before physical-device experimental development (read-only research and
disposable emulator work may proceed independently):

1. Identify the exact device variant.
2. Record its current software/firmware state.
3. Preserve all available firmware and ROM material.
4. Preserve relevant filesystem/device data where possible.
5. Hash preserved artifacts.
6. Document recovery procedures.
7. Establish a known-good baseline.

Create an independently held offline reference copy and record its digest.
Owner-reversible read-only permissions alone do not satisfy immutable preservation.

Prefer having a second Nokia 808 for experimentation if practical:

```text
808-A
    reference / preservation device
    normally never modified

808-B
    development device
    may run experimental applications/system components
```

The project should always retain a known-good recovery path independent of the continued health of the development phone.

---

# 6. Strict hardware safety boundary

The coding agent must not be given unrestricted control over the physical phone.

Divide hardware operations into tiers.

## Safe operations

Examples:

```text
device info
device logs
device screenshots
install ordinary application
uninstall ordinary application
run application
collect diagnostics
```

These may eventually be automated.

## Dangerous operations

Examples:

```text
reboot
modify system files
install system components
replace system DLLs
modify persistent system state
```

These require explicit human authorization.

## Recovery / irreversible operations

Examples:

```text
flash firmware
erase storage
modify bootloader
partition storage
modify OTP
modify calibration data
perform low-level recovery
```

These must **not be agent capabilities**.

The agent may prepare artifacts or instructions for such operations, but the actual operation must remain outside the agent's automated authority.

Do not rely solely on instructions in the agent prompt for this protection.

Enforce the boundary in the tooling itself.

---

# 7. EKA2L1 should be the primary experimental device

Investigate and use EKA2L1 as the principal Symbian emulator.

The emulator should become a first-class development target, not merely a convenient way to run an occasional application.

Build a macOS-native orchestration layer around it.

Desired interface:

```bash
symbian emu create 808
symbian emu start
symbian emu reset
symbian emu snapshot save clean
symbian emu snapshot restore clean
symbian emu install foo.sis
symbian emu uninstall foo
symbian emu launch foo
symbian emu stop foo
symbian emu screenshot
symbian emu logs
symbian emu trace
symbian emu dump
```

Investigate the emulator's actual capabilities before inventing additional infrastructure.

Where necessary, contribute improvements to the emulator rather than building external workarounds.

---

# 8. Emulator state must be disposable

Create a golden emulator state.

Tests should work approximately like:

```text
golden Belle image
       |
       v
disposable test instance
       |
       +-- install
       +-- execute
       +-- modify
       +-- crash
       +-- reboot
       +-- inspect
       |
       v
discard
```

A broken test must never poison subsequent tests.

The coding agent should be able to perform aggressive experiments in the emulator without fear of permanently corrupting the development environment.

Snapshots, filesystem copies, deterministic reset mechanisms and reproducible emulator images should be preferred over manual cleanup.

---

# 9. Build a modern Symbian platform facade

Create a repository that hides historical complexity behind modern interfaces.

Conceptually:

```text
symbian-2026/
    sdk/
    toolchain/
    build/
    packaging/
    emulator/
    debugger/
    analysis/
    device/
    tests/
    tools/
    docs/
    recovery/
```

The precise layout is yours to determine.

The important architectural distinction is:

```text
modern developer interface
          |
          v
platform implementation
          |
          v
historical ABI / hardware
```

not:

```text
developer
   |
   v
historical Nokia build environment
   |
   v
historical Nokia tools
```

---

# 10. Investigate the compiler/toolchain rather than assuming it

Determine what is actually necessary to produce valid Symbian binaries for the target.

Investigate, in order:

1. Modern LLVM/Clang capabilities.
2. Whether contemporary LLVM can generate the required ARM architecture and ABI.
3. Whether Symbian-specific ABI requirements can be supported directly.
4. Whether the old GCCE toolchain can be extracted and run natively on macOS.
5. Whether only a small part of the historical compiler infrastructure is actually needed.
6. Whether individual missing functions can be reimplemented.
7. Only then, if genuinely necessary, whether a historical executable needs a compatibility environment.

Do not preserve GCCE merely because it was historically used.

Conversely, do not rewrite it prematurely if it turns out to encode important ABI behaviour that is difficult to reproduce correctly.

The question is always:

> What exact technical property do we need from this tool?

---

# 11. Separate source intelligence from target compilation

Modern code intelligence should not depend on the historical compiler.

Aim for:

```text
                         source
                           |
                           v
                     clangd / LLVM
                           |
        +------------------+------------------+
        |                  |                  |
    diagnostics        navigation         indexing
    references         completion          analysis
    refactoring        call hierarchy      symbols
                           |
                           v
                    target compiler
                           |
                           v
                     Symbian binary
```

Provide accurate enough compilation databases and platform descriptions for clangd.

The developer should get modern:

* jump to definition
* find references
* completion
* diagnostics
* call hierarchy
* symbol search
* semantic highlighting
* static analysis
* refactoring support

even if the final target binary is produced by an ancient-compatible backend.

---

# 12. Rebuild historical utilities when practical

Investigate each historical utility individually.

For example, if an old utility converts:

```text
.pkg -> .sis
```

determine what it actually does.

If the functionality is well understood and feasible to implement:

```text
modern native implementation
```

is preferable to:

```text
Windows VM
    -> old Nokia executable
    -> output
```

The old executable can then become a compatibility oracle used in tests.

This principle applies to:

* package generation
* resource processing
* metadata generation
* symbol handling
* signing-related preparation
* build orchestration
* emulator configuration
* filesystem manipulation
* diagnostic extraction

Do not undertake unnecessary rewrites merely for ideological purity. The purpose is to reduce complexity and gain understanding.

---

# 13. Treat the historical SDK as research material

Do not blindly import the entire historical SDK.

For every SDK component, determine:

```text
What is this?
Why does it exist?
Who consumes it?
What contract does it provide?
Is that contract still needed?
Can we replace it?
Can we simplify it?
Can we expose it through a modern interface?
```

The desired outcome is not:

> "We have successfully reproduced the entire Nokia SDK."

The desired outcome is:

> "We understand which parts of the Nokia SDK are actually required, and we have the smallest maintainable implementation providing those capabilities."

---

# 14. Preserve compatibility where it has genuine value

There are cases where historical compatibility is itself important.

For example:

* existing Symbian binaries
* established Symbian ABI conventions
* Belle system APIs
* application package formats
* filesystem formats
* IPC protocols
* hardware interfaces
* binary layouts
* documented or de facto application behaviour

Preserve these where they are required to interoperate with the target.

But distinguish:

```text
compatibility required by the target
```

from:

```text
compatibility accidentally inherited from Nokia's development process
```

Only the former should automatically survive.

---

# 15. Build modern debugging and observability

A major goal is to provide a **2026-level debugging experience** even where the original platform did not.

Develop infrastructure for:

* symbols
* stack traces
* symbolication
* process inspection
* thread inspection
* memory inspection
* crash dumps
* logs
* tracing
* IPC tracing
* filesystem activity
* screenshots
* input capture
* deterministic reproduction
* emulator snapshots
* test artifacts

For example, an agent should eventually be able to receive something like:

```text
Process: MyServer.exe
Thread: WorkerThread
Panic: KERN-EXEC 3

Stack:
    MyServer::HandleMessage()
    MyServer::Dispatch()
    RMessage2::Complete()
    ...

Source:
    server.cpp:418

Registers:
    ...

Loaded modules:
    ...
```

rather than only:

```text
KERN-EXEC 3
```

---

# 16. Build an observability layer for new code

For components under our control, define modern diagnostic abstractions that can map to different backends.

For example:

```cpp
SYMBIAN_LOG("camera", "frame received");
SYMBIAN_TRACE_SCOPE("RequestHandler");
SYMBIAN_ASSERT(condition);
```

On the phone this might ultimately use historical facilities such as RDebug, files, serial output, or custom IPC.

In the emulator it could produce structured host-side events.

Do not prematurely imitate modern observability standards if the target makes that inappropriate. The important property is that diagnostics become machine-readable, correlated and useful to both humans and coding agents.

---

# 17. Make the development loop agent-friendly

The platform should expose operations as composable, machine-readable tools.

A coding agent should eventually be able to do:

```text
build
    |
    v
run unit tests
    |
    v
package
    |
    v
create clean emulator
    |
    v
install
    |
    v
launch
    |
    v
exercise UI
    |
    v
collect screenshot/log/crash
    |
    v
inspect
    |
    v
modify source
```

Avoid requiring agents to scrape terminal output or interact with opaque GUI tools where a structured interface can reasonably be provided.

Prefer JSON/structured output for tooling APIs.

---

# 18. Do not optimise for literal historical UI/API reproduction

The user wants to develop applications for the 808, not recreate every aspect of Carbide.c++, Nokia Developer Tools or the historical desktop experience.

It is fine—even preferable—to provide:

```text
VS Code / CLion / another modern editor
+
clangd
+
modern CLI
+
modern debugger
+
modern emulator UI
```

rather than reproducing historical IDE functionality.

Likewise, if a historical SDK GUI merely wrapped a straightforward command-line operation, replace it with a modern CLI/API.

---

# 19. Keep an explicit research log

For significant discoveries, record:

```text
Question
Historical behaviour
Evidence
Experiments
Conclusion
Implementation decision
Remaining uncertainty
```

This project will involve archaeology and reverse engineering.

Do not let assumptions become undocumented architecture.

If you discover that an apparently essential Symbian component is unnecessary, record that discovery.

If you discover that a bizarre historical mechanism exists because of a real ABI or hardware constraint, record that too.

---

# 20. Use experiments to answer architectural questions

When uncertain, construct small experiments.

Examples:

* Can Clang produce binaries accepted by the Symbian loader?
* Which ARM ABI properties actually matter?
* Which Belle libraries are genuinely required for a minimal application?
* What does a SIS package actually contain?
* Which parts of the historical signing pipeline are necessary?
* Which EKA2L1 behaviours differ from the 808?
* Which system APIs are actually required by a Qt application?
* Can a system server be replaced without reproducing unrelated SDK infrastructure?
* Can an old utility's functionality be independently reproduced?
* Which hardware interfaces are accessible from an alternative environment?

Prefer a small experiment over speculative architecture.

---

# 21. Establish milestones

Work incrementally.

## Milestone 0 — Device preservation

Current evidence: archive inventory/hash tools and supplied material are tested;
physical identity, installed firmware, independent offline copy and recovery
appliance remain pending. This is not complete.

Produce:

* exact device identification
* known-good firmware inventory
* immutable firmware/archive
* recovery documentation
* recovery VM
* hardware safety model

Do not experiment with firmware yet.

## Milestone 1 — Native macOS toolchain

Current evidence: the limited LLVM/E32 toolchain is reproducible and independently
validated; ROMless probes and one guarded SDK GUI run pass. General ABI, runtime
and physical compatibility remain acceptance gates.

Produce:

* initial ARM/Symbian toolchain investigation
* minimal compiler/build path
* modern build wrapper
* first reproducible binary

## Milestone 2 — Modern project model

Current evidence: CMake/Ninja, compilation databases, native single-EXE packaging
and standalone ARM GUI IDE setup exist. General project/resource/package profiles
and the actual CLion UI experience remain work.

Produce:

* CMake or equivalent project integration
* compilation database
* clangd support
* modern diagnostics
* package generation

## Milestone 3 — Emulator

Current evidence: native arm64 frontend, preserved ROM/Z import, copied instances
and real GUI pixels/input/zero exit pass on both backends. Native control exposes
status/capture/pointer only; general lifecycle/reset/snapshot orchestration and
full OS boot remain incomplete.

Produce:

* EKA2L1 setup
* Belle environment
* automated installation
* launch
* reset
* snapshots
* screenshots
* logs

## Milestone 4 — Automated application development

Current evidence: build/package/verification and opt-in live GUI Pytest loops
exist. `symbian test` and the general unattended install/run/artifact loop are
not yet implemented; make these the next developer-facing integration slice.

Produce:

```bash
symbian build
symbian test
```

where tests actually execute applications in the emulator.

## Milestone 5 — Modern debugging

Current evidence: relocated live ARM GDB source/ROM stops, stable single stepping
and native process-exit records pass. Full stack traces, panic symbolication,
thread/module inspection and CLion remote-debug validation remain open.

Produce:

* symbols
* crashes
* stack traces
* process/thread inspection
* useful agent-readable diagnostics

## Milestone 6 — Physical-device application deployment

Allow controlled installation of ordinary applications onto the development 808.

The application-menu resource slice now exposes project-relative SVG icons
and BCP 47 keyed translated captions in `[application]`. The SDK compiles
UTF-8 Unicode resources and a deterministic SVG-in-MIF icon, hiding resource
script and SIS language syntax. Native package checks and the original
EKA2L1 installer, AppArc and MIF reader accept the seven-file `gui_app` package
on both backends. Physical Belle icon rendering and in-app localization
remain open gates.

The emulator remains the default.

The proposed resident development service follows the migration gate in
[`DEVELOPMENT_AGENT.md`](DEVELOPMENT_AGENT.md): prepare and fault-test its
minimal authenticated, read-only service in disposable emulator instances.
The default SDK now builds the vendored Mbed TLS port for both ARM profiles as
opt-in static targets and includes its source tree. A per-project CMake CA
bundle setting packages only selected roots and records their digest; an unset
project gets no bundled roots. Phone-side entropy, UTC, certificate, socket
and authenticated TLS 1.2/1.3 handshake gates precede Wi-Fi service
availability. See [docs/MBEDTLS.md](docs/MBEDTLS.md). After
those checks,
verify its ARM image and ordinary SIS; then manually install it on the
development phone with an independently held offline baseline. Prove local
disable/uninstall, startup, permissions, idle cost and connection-loss
behavior on that handset before enabling boot start or adding file transfer,
deployment and debugging. An emulator pass or ELF build alone does not
authorize or establish this phone result. Keep the preservation phone out of
the first service installation.

The first 2026-10-02 slice has a serial-redacted USB/volume inventory and an
SDK-build/native-SIS-checked mass-storage staging command. It reports
`awaiting-on-device-install`; a storage copy does not prove installer acceptance
or execution. A second slice packages original-`rcomp` application-menu
registration and English captions from `[application]` in `symbian.toml`;
`gui_app` and new `symbian init` projects both declared them. That initial
three-file
package passes native hash/UID/path checks and a fresh installed-SDK build,
and the corrected Unicode resources pass EKA2L1's original AppArc caption
parser plus headless installer/registry checks. After the owner's photo copy,
the SDK staged and SHA-256-verified an ARMv5T portable SIS on the phone and
ejected its USB disk. Its state is `awaiting-on-device-install`; handset menu
appearance was then confirmed by the owner after manual installation. The CLI
cannot yet collect that result automatically. The owner also reported that
the app opened and responded to a tap; this is physical user observation,
not an instrumented compatibility test. Next: observe the on-phone
installer result and exact OS/RM/
runtime state without invasive probes, then add a verified direct installer
transport if available. Keep connection identity, volume selection, package
provenance and installation state explicit. The Linux sysfs/mount adapter now
passes synthetic controls; validate it with a real handset before claiming
Linux device support. Add capability-gated screenshot/debugging later.
Firmware/recovery remains a separate human-governed broker, with no agent
execution endpoint. Preserve the physical baseline and offline copy gate.

## Milestone 7 — System development

Support:

* servers
* DLLs
* plugins
* IPC
* system components
* lower-level APIs

Primarily tested in EKA2L1.

## Milestone 8 — Hardware research

Investigate lower-level hardware interfaces and firmware only after the preceding infrastructure is reliable.

## Milestone 9 — Alternative operating system

Treat this as a separate project built on the accumulated hardware knowledge.

---

# 22. Alternative OS work

Eventually it may be desirable to run another OS on the Nokia 808.

Do not begin by attempting to reproduce Symbian.

A first meaningful milestone might be:

```text
boot
  |
  v
minimal ARM environment
  |
  v
memory management
  |
  v
scheduler
  |
  v
display/framebuffer
  |
  v
input
```

The first goal could literally be:

> Boot the Nokia 808 and display a diagnostic message without modifying or depending on the existing Belle filesystem.

Investigate the boot chain, hardware and firmware scientifically.

Keep alternative-OS work isolated from the ordinary application-development pipeline.

---

# 23. Quality bar

Do not declare success merely because an old application launches.

The resulting platform should aspire to modern engineering properties:

### Reproducibility

The same source and toolchain should produce reproducible artifacts wherever practical.

### Isolation

Emulator experiments must be disposable.

### Observability

Failures should produce useful machine-readable diagnostics.

### Automation

Build/test/deploy/debug should be scriptable.

### Source intelligence

Modern code navigation and static analysis should work.

### Documentation

Important reverse-engineering discoveries should be recorded.

### Minimalism

Do not retain components merely because they were historically present.

### Safety

The physical phone must have strong operational boundaries.

### Maintainability

Prefer understandable modern code over opaque compatibility machinery.

---

# 24. How to make decisions

When choosing between:

### A — Preserve historical implementation

### B — Wrap historical implementation

### C — Reimplement the underlying functionality

### D — Eliminate the functionality entirely

Do not automatically choose A.

Instead investigate the actual requirement and choose the simplest option that preserves the required behaviour.

A useful decision order is:

```text
Is the functionality actually needed?
        |
       no ──────> eliminate it
        |
       yes
        |
Can modern native code implement it cleanly?
        |
       yes ─────> reimplement
        |
       no
        |
Can a small compatibility layer expose the necessary contract?
        |
       yes ─────> compatibility layer
        |
       no
        |
Is the historical implementation genuinely required?
        |
       yes ─────> preserve/use it
```

Even when preserving something, isolate it behind a clean modern interface.

---

# 25. Do not prematurely build everything

This is a very large project.

At every stage:

1. Establish the smallest useful capability.
2. Test it against real evidence.
3. Document what was learned.
4. Remove unnecessary dependencies.
5. Only then expand the scope.

Avoid building speculative infrastructure for capabilities that have not yet been demonstrated to be necessary.

---

# 26. Working procedure after the initial survey

The initial survey is recorded in docs/RESEARCH.md and subsequent experiments in
docs/RESEARCH_LOG.md. Resume from the implementation review and current STATUS,
inspect the workspace for unfinished changes/processes, then pursue the next
unmet evidence gate. Do not repeat completed archaeology without a new question.

Keep the research document current as scope expands, covering:

1. What is already available.
2. What historical Symbian/Belle components appear genuinely necessary.
3. Which components can plausibly be replaced with modern native implementations.
4. Which historical components may require compatibility treatment.
5. What EKA2L1 already provides.
6. What must be added around EKA2L1.
7. What is required for a native macOS toolchain.
8. What hardware preservation/recovery information is needed.
9. Which operations must remain outside agent authority.
10. A proposed sequence of small, testable milestones.

Do not immediately start implementing a giant compatibility layer.

First understand the system.

When you do begin implementation, prefer small vertical slices that prove important assumptions.

The guiding question throughout the project is:

> **What is the minimum modern machinery required to make the Nokia 808 a genuinely pleasant, deeply observable, programmable computer in 2026?**

Build that—not a museum replica of Nokia's development environment.
