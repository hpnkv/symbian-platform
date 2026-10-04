# Writing a C++ application for Symbian in 2026

Device selection is independent of application and SDK artifacts. See
[ROM/Z configuration and transfer](../guides/firmware.md). The SDK resolves settings,
verifies firmware and owns fresh emulator copies; application code does not need
ROM paths, firmware-specific executive maps or display-scale calculations.
Modern features supplied by the SDK stay available on older systems when their
actual ABI/services permit them. Current generated starters execute on C7, E6,
6120 and E71; EKA1 startup/import adaptation remains absent and is reported
before Run. Unsupported font-server calls return a real error instead of hanging.

The ordinary `symbian init` starter uses `Symbian::Stackless`: a tap logs now,
then a timer Future logs again, and Clear cancels pending work. Authors work
with Tasks and `absl::StatusOr` in typed code; the SDK bridge owns the native
`RTimer`, request status, semaphore wait and Window Server cleanup. If an
explicitly selected firmware has no `libpthread.dll`, init chooses the portable
profile; `--portable-runtime` makes the same choice without firmware. Run
checks imported services before starting the emulator. This starter path is
stackless. The separate `Symbian::Fibers` profile can park an unresolved
`Await` inside a guest fiber; outside one it fails clearly.

Write your application logic in ordinary modern C++. Use standard strings and
containers, Abseil for status/maps, and nlohmann::json where the selected **guest**
runtime supports them. Keep platform-specific types and calls in small adapters.
You should not need to learn executive call numbers, construct E32 headers, or
manage compiler relocation workarounds to write an application. Those belong to
the SDK. This guide covers decisions the application still has to make.

The SDK is under construction. C++20 language support, a runtime capability, and
an OS service are separate promises. The current string/vector runtime is a
bounded execution-tested subset; guest Abseil Status/StatusOr and
`flat_hash_map` have bounded installed-SDK execution tests, while guest JSON,
broader Abseil, full A11 scheduling and fiber lifetime, and many hosted
facilities still need ports. Shared/unique ownership
and a bounded A11-derived stackless Future/Task subset have guest controls.
Bounded global initialization and one parent/worker thread join have guest
execution tests. Host tooling using those libraries
does not make them available inside the phone. Check [RUNTIME.md](runtime.md)
and [STATUS.md](https://github.com/hpnkv/symbian-platform/blob/main/.dev/status.md) before selecting a feature.

## Keep application logic away from the platform ABI

The application executes as 32-bit ARM code. Pointer and integer sizes differ
from a modern desktop host; use fixed-width types for persisted/wire data and
check ranges before passing sizes to APIs taking signed TInt. Never serialize a
pointer or copy a host-built C++ object's memory layout into a guest structure.
A host GTest passing on a 64-bit Mac cannot establish a 32-bit SDK layout.

System DLLs have fixed calling conventions, object layouts and numbered exports.
New compiler/STL objects are suitable inside your own consistently built code;
they are not a replacement ABI for existing OS DLLs. Pass the platform's expected
descriptor/handle or a narrow C interface at the boundary. Do not expose
std::string, std::vector, Abseil maps or nlohmann objects across a frozen system
DLL interface, and do not assume two separately built runtime configurations are
interchangeable. [The image contract](https://github.com/SymbianSource/oss.FCL.sf.os.kernelhwsrv/blob/0c3208650587ac0230aed8a74e9bddb5288023eb/userlibandfileserver/fileserver/inc/f32image.h)
is independent of the host compiler.

The SDK should select the triple, headers, runtime configuration and import
proxies, and reject unsupported binaries with useful diagnostics. Application
code should never embed syscall numbers or copy functions from the emulator.
The current runtime's SDK adapter also isolates a real placement-new header
conflict; see RUNTIME.md. Do not work around that conflict by editing OS headers
or disabling exception-specification checks in your application.

## Treat descriptors as views or buffers with an explicit encoding

Application-menu captions are configured in `symbian.toml` with fallback
text and optional BCP 47 keyed translations. An SVG icon is a
project-relative asset. The SDK compiles these to native resources and a
MIF container; application C++ does not need RSS or SIS language syntax.
This is menu localization only. In-app text still needs an owned UTF-8
model and a tested conversion at its UI boundary.

A descriptor carries a length; writable descriptors also have a capacity. It is
not a C string and need not be NUL terminated. Its length is a count of 8-bit or
16-bit units, according to its type. Byte descriptors are also used for binary
data, so an 8-bit descriptor does not imply UTF-8. The API specifies the encoding.

Keep UTF-8 std::string in your model when that is your application's text
convention. Convert to the required platform encoding immediately before a call
and convert results immediately after it. For 16-bit text, account for surrogate
pairs; byte counts, UTF-16 unit counts and user-visible character counts differ.
Check capacity instead of assuming an oversized copy will return a normal error.
The preserved [8-bit](https://github.com/SymbianSource/oss.FCL.sf.os.kernelhwsrv/blob/0c3208650587ac0230aed8a74e9bddb5288023eb/kernel/eka/include/e32des8.h)
and [16-bit descriptor interfaces](https://github.com/SymbianSource/oss.FCL.sf.os.kernelhwsrv/blob/0c3208650587ac0230aed8a74e9bddb5288023eb/kernel/eka/include/e32des16.h)
define these contracts.

A pointer descriptor can borrow memory. Keep that memory alive and stable for
as long as the OS may use it. A temporary conversion is sufficient for a
synchronous call that consumes it immediately; it is insufficient for an
asynchronous call retaining the buffer. Do not mutate/reallocate a string or
vector while its storage backs an outstanding request.

The SDK should provide encoding-aware, capacity-checked adapters. Even with
those adapters, your application must choose its encoding and retain ownership
until completion. Qt helpers follow the same boundary rule: use QString/QJson
only where an external Qt API actually requires them.

## Distinguish returned errors, leaves and panics

These are different control-flow contracts:

| Result | Application responsibility |
| --- | --- |
| Returned TInt / completed request error | Check it and translate into the application's status policy at the adapter |
| A leaving API, usually suffixed `L` or `LC` | Call only through a verified leave/cleanup boundary |
| Panic | Treat as a failed invariant/programming contract and retain the diagnostic |

A leave is not automatically an Abseil Status and is not a portable C++
exception. An `LC` function additionally transfers a cleanup-stack obligation.
Do not assume std::unique_ptr destructors or a C++ catch block implement a raw
Symbian leave boundary. The preserved [cleanup and trap interfaces](https://github.com/SymbianSource/oss.FCL.sf.os.kernelhwsrv/blob/0c3208650587ac0230aed8a74e9bddb5288023eb/kernel/eka/include/e32base.h)
must be respected by the adapter.

The SDK's host native libraries disable C++ exceptions and return
Status/StatusOr. Guest applications currently compile with exceptions off by
default. An opt-in guest exception profile is planned, but is not available.
An isolated probe now verifies ARM unwind-table and E32 descriptor publication
and a no-throw cleanup path. Imported type-info data and actual throw/catch
destructor unwinding remain unverified. Even then, a Symbian leave remains a
separate contract.
Guest leave-to-Status adapters are not yet generally verified. Prefer the proven
nonleaving APIs in the examples until that boundary is supplied. Do not rename
an `L` call or ignore its failure to make a no-exceptions build pass.

Allocation policy also matters. The initial guest libc++ profile makes ordinary
new terminate the guest with KErrNoMemory; nothrow allocation returns nullptr.
A container's allocation failure therefore does not currently become a
recoverable StatusOr. Bound input sizes and working sets. If your application
must recover from memory pressure, require a tested fallible allocation API
before choosing that container for the operation. RUNTIME.md records the exact
current policy. `std::nothrow` now works directly in the bounded guest profile;
the SDK relocates its local GOT entry. Generated model creation checks null
and releases acquired native resources. This fallible acquisition does not
make subsequent string/vector growth recoverable.

## Static storage and lifecycle

New projects use the SDK's verified independent code/data linker layout.
New projects target ARMv6 by default; choose ARMv5T for a device or ROM profile
that requires it. The selection controls compiler attributes, E32 CPU metadata
and the guest runtime archive. The SDK checks ELF attributes before publication
and launch. A ROM's product name alone is not a reliable CPU/ABI proof; inspect
the device target when hardware compatibility matters.
Constant-initialized mutable EXE globals and zero-initialized BSS now work;
pointers to code/data receive loader fixups automatically. Do not manually put
writable storage into the code segment or adjust runtime addresses. The current
bounded profile limits combined data/BSS to 1 MiB. The SDK startup now runs
bounded global constructors after heap setup and global destructors before exit;
real `std::string` globals execute on both ARM profiles and emulator backends.
Local-static guards, TLS and full DLL lifecycle still need runtime work.
Unsupported sections and relocation forms are errors, not silently removed.
See RUNTIME.md for execution and failure controls.

The SDK now preserves Clang's ARM variadic-call rules when OpenC headers are
included, and the guest has a tested `std::error_code` category/message path.
The acceptance calls the named firmware's real `libc.dll` for `vsnprintf` and
`strerror_r`; another firmware profile must supply compatible C exports for
those operations. The alternate installed `Symbian::Streams` target supports
a bounded classic-C locale and `std::ostringstream` formatting; select it
instead of `Symbian::Runtime` so its libc++ configuration matches its archive.
C/POSIX names are supported; file streams, general wide I/O and other named
locales remain unverified. `Symbian::AbseilStatusOr` links the matching streams
runtime and supplies the tested Status/StatusOr and flat-hash-map subset.
Imported function pointers have
a tested bounded PLT/data-relocation path; imported data objects, including
exception typeinfo, still need a
separate ABI implementation.

`EPOCALLOWDLLDATA` was an MMP converter opt-in for writable DLL `.data` and
`.bss`; the original `elf2e32` rejects those sections in a DLL without it.
It does not make executable data relocation or DLL initialization automatic.
This SDK has verified EXE data and a bounded DLL case: simple initialized and
zeroed globals in a DLL get per-process storage and relocation on both emulator
CPU backends. A C++ DLL constructor also runs on process attach through the
real Belle static-call list. A bounded dynamic-load/close case also runs a DLL
destructor before `RLibrary::Close` returns. TLS and general DLL lifetime are
still open. Keep mutable DLL globals default-visible when the toolchain must
emit GOT references; internal/hidden cross-mapping references currently fail
conversion with an explicit error. Applications need no historical MMP setting.
Generated projects receive the tested `RLibrary::Load`, `Lookup` and `Close`
imports from the SDK's EUSER proxy; they need no hand-written ordinal file for
this path. Check the selected firmware's actual ABI before assuming this
named-fixture result applies to another ROM.

## An asynchronous request owns its storage until it completes

Symbian services commonly submit work using a TRequestStatus that completes
later. A pending status, descriptor, receive buffer or referenced object cannot
be a temporary that disappears when the initiating function returns. Keep a
clear owner for the operation and its memory.

Model cancellation as a state transition: request cancellation, establish that
the outstanding request has completed, then release its buffers/status and close
its handles. The API-specific cancellation contract decides how completion is
observed. Cancellation requested is not the same as ownership released.
[Request-status and wait interfaces](https://github.com/SymbianSource/oss.FCL.sf.os.kernelhwsrv/blob/0c3208650587ac0230aed8a74e9bddb5288023eb/kernel/eka/include/e32std.h)
are part of the platform boundary.

Keep the UI/event loop responsive. Long computation, filesystem work or a wait
inside the dispatcher delays unrelated events and redraws. Raw active objects
need a scheduler and define their RunL/error/cancellation behavior; ordinary
C++ code does not acquire those semantics by declaring a callback. Use the SDK's
supported event-loop adapter when available. A future guest concurrency adapter
must use A11's thread library. A bounded `std::thread` path now executes on a
ROM with `libpthread.dll`: creation, join, atomic updates, and shared/unique
ownership across a worker. Link `Symbian::Threads` for that profile and check
the selected firmware provides its imports. On the tested RM-807 ROM,
`std::this_thread::yield()` also imports `sched_yield` from `libc.dll`.
The default no-exceptions runtime terminates with -6 for an invalid
`std::thread::join()`; handle thread ownership before calling `join()`.
`Symbian::Stackless` adds the
bounded A11-derived Promise/Future/Task and nonblocking fan-in subset. The
installed `<symbian/concurrency/native_timer.h>` owns one native `RTimer`
request and drains cancellation on close; create, arm and close it on one OS
thread. `TimerPump` in `<symbian/concurrency/timer_pump.h>` now completes
bounded timer Tasks, including worker-requested cancellation, on the owning
event thread. It does not yet own a shared Window Server event pump or
arbitrary native service requests, and there is no complete event-loop adapter,
stackful fiber or verified `jthread`. For 64-bit atomics, the default
runtime uses an `RFastLock`. `Symbian::NativeAtomics64` calls ROM atomics and
should be selected only for a verified firmware. The imported RM-807, RM-675
and RM-609 ROMs pass its cross-thread emulator controls; RM-243 and RM-346
lack its EABI imports. Lock-free behavior can vary by ROM implementation.
The SDK's `is_lock_free()` query reflects the selected
runtime archive. Clang's raw `__atomic_always_lock_free` target query may not
describe the linked SDK archive; use the standard runtime query for a concrete
atomic object.
For application deadlines, use `absl::Time` for an absolute real-world time
and `absl::Duration` for a relative delay. `TimerPump` converts an accepted
absolute time once to monotonic waiting; a later wall-clock correction does
not move that request. Do not replace `absl::Now()` with a raw counter or
read native counters in application code. The ROM's OpenC `CLOCK_MONOTONIC`
fails on the tested RM-807 fixture;
the SDK clock uses the nanokernel tick and its HAL period, with an ordinary
tick fallback. Its precision depends on the device, and a gap of half a
32-bit counter wrap between reads still needs further validation (about
24.9 days at a 1 ms nanokernel tick). `FastCounter` is
suited to short measurements only after checking that device's frequency,
direction and power behavior; it is not the default deadline source.
Creating a second scheduler is not a shortcut to compatibility.

The current gui_app uses a deliberately small paired event/redraw wait loop.
Its pending statuses remain alive and it cancels/drains requests during cleanup.
The generated starter has an opt-in `SYMBIAN_ENABLE_TIMER_TASKS` profile whose
one wait services those statuses and timer Tasks. It requires firmware
`libpthread` and passed RM-807 Dynarmic/Dyncom GUI controls. This is a bounded
example, not a general asynchronous framework. `TimerPump` defaults to 64
pending timers; scheduling at capacity yields a ready Task with an explicit
resource-exhausted result. Observe Task results even when scheduling appears
to complete immediately.
`PropertyWatch` owns one Publish & Subscribe change request and yields a
`Future<int>`; it dispatches beside the timer pump before the single event
thread wait. `EventMailbox` provides bounded event-thread callbacks; it is
not A11's shared-pool `Post`. An unresolved `Future::Await` returns a clear
error outside a fiber; inside a guest `thread::Fiber` it parks cooperatively.
Guest `thread::Mutex`, `MutexLock`, `CondVar` and `SleepFor` now switch to
pending fibers instead of blocking the event OS thread. The separate
`Symbian::Fibers` archive exposes an explicitly pumped, pinned scheduler with
custom ready ordering and wake notification. It does not yet integrate with
the native request owner or implement A11 `PermanentEvent`, `Select`,
structured joining or cancellation. Stack guards, native leave/TRAP
boundaries and debugger-visible waits also remain unverified. Keep each
`thread::Fiber` and its scheduler alive until the fiber finishes: the current
bounded destructor treats unfinished ownership as a runtime contract failure.

## Close handles and keep the allocator owner explicit

A platform session or R-class handle commonly needs Close(); do not infer that
its C++ destructor closes it. Treat a successful acquisition as creating a
cleanup obligation, including partial initialization failures. Use verified RAII
wrappers where supplied, or explicit cleanup in a small adapter. Follow the
specific API for whether the handle is process/thread owned or transferable;
copying the numeric value is not a general ownership transfer.

Allocate and free through the same ownership domain. Do not delete a platform
allocation using an unrelated modern allocator or export allocator-owned objects
across DLL boundaries without an agreed release API. Symbian has thread heaps;
secondary-thread heap/TLS setup is a runtime concern, not something achieved by
jumping into a C++ function. Generated startup now sets up a secondary thread's
heap and calls its entry function; one bounded parent/worker join executes.
General TLS cleanup, cross-thread freeing and worker lifecycle remain open.
The [heap/session interfaces](https://github.com/SymbianSource/oss.FCL.sf.os.kernelhwsrv/blob/0c3208650587ac0230aed8a74e9bddb5288023eb/kernel/eka/include/e32cmn.h)
make these distinctions explicit.

Prefer explicit lifetime for application services. The SDK now handles bounded
EXE global construction/destruction, DLL process-attach construction and one
dynamic close/destructor contract. TLS objects and general DLL teardown ordering
still need verified adapters. Moving
a mutable global into a custom ELF section is not a legitimate fix. Until the
remaining lifetime contracts are supported, keep service state in its owner.

## Plan installation identity and permissions before relying on a service

An executable's identity, package identity and permissions are distinct inputs.
UID/SID identify components and security principals; capabilities authorize
protected operations, and a package's certificate/signing policy affects what
the installer permits. Setting a capability bit in a build manifest is not a
grant from the physical phone. Request the smallest capability set needed by
the application's actual features. The preserved [capability definitions](https://github.com/SymbianSource/oss.FCL.sf.os.kernelhwsrv/blob/0c3208650587ac0230aed8a74e9bddb5288023eb/kernel/eka/include/e32capability.h)
are the API vocabulary; device policy still has to be tested.

The current examples use experimental E-range UIDs. Do not ship those identities
as a released application allocation. Installation, application registration,
launch-menu visibility and starting an executable by path are separate gates.
A Window Server client drawing a window is not automatically a fully registered
application with lifecycle integration. The SDK now owns menu registration
resources, translated captions and icon packaging; broader lifecycle
integration is still bounded. You must still select a stable application
identity and permission policy.

Keep executable code and writable application data separate. Symbian paths are
drive-based; Z is the ROM view, system executables use `sys/bin`, and private
application data belongs in the appropriate security-controlled location.
Do not hard-code C as the only writable volume or depend on a desktop working
directory. Removable/absent/full storage and failed writes are normal conditions.
The SDK should offer path/service adapters; the application still chooses which
data is durable, private, exportable or disposable. The [file-server interface](https://github.com/SymbianSource/oss.FCL.sf.os.kernelhwsrv/blob/0c3208650587ac0230aed8a74e9bddb5288023eb/userlibandfileserver/fileserver/inc/f32file.h)
is the platform boundary, not POSIX filesystem equivalence.

## Assume backgrounding, redraw and interruption are ordinary operation

Keep durable state in the model and draw from that model when requested. Do not
use the framebuffer as your source of truth or assume a redraw arrives once.
Use logical screen coordinates and the actual orientation/size; emulator PNG
pixel dimensions may have a display scale. Handle focus changes, repeated input,
resource loss and orderly shutdown without losing ownership of pending work.
The [Window Server interface](https://github.com/SymbianSource/oss.FCL.sf.os.graphics/blob/ff133bc50e6158bfb08cc093b0f0055321dcde99/windowing/windowserver/inc/W32STD.H)
is where rendering/event lifetimes meet the OS.

A phone has limited memory, storage and energy. Keep queues bounded, avoid busy
polling and work in bounded chunks. Persist important user data at deliberate
points; a destructor at process exit is not your only durability strategy.
Desktop-fast code or continuous redraw can still be a poor phone application.

## Test the boundaries, not just the model

Host GTest validates portable model behavior; emulator tests validate actual
loader/imports, heap/service calls, event lifetimes and rendered results. Use
Pytest to install/launch in an owned fresh instance, exercise input, retain
captures and inspect native exit/panic records. Include allocation failure,
invalid input, cancellation during shutdown and repeated acquisition/cleanup.
A successful ELF build or E32 parser check establishes neither runtime execution
nor the phone's installation policy.

Keep the debug ELF exactly paired with the executable installed for the test.
Runtime load addresses can differ from link addresses; let the SDK relocate
symbols instead of hard-coding addresses in breakpoints. A source change needs
a newly published ELF/E32 pair. The root `gui_app_run` supervisor performs that
publication; see [CLION.md](../guides/clion.md).

The original counter/runtime acceptance remains guarded against one preserved RM-807 firmware
fixture and uses an explicit experimental routing profile. The physical phone's
identity/firmware is still unknown. Do not infer general Belle or physical-device
compatibility from that run. Those differences should be diagnosed by SDK
profiles and tests, rather than leaking firmware workarounds into app logic.

## What belongs to the SDK as it matures

For bounded stackless composition, link `Symbian::Stackless` and include
`<symbian/concurrency/future.h>` or `<symbian/concurrency/parallel.h>`. A producer
completes its Promise from an OS completion callback or a verified worker;
`Then`, `JoinAll`, `TaskGroup::Finish` and bounded `DriveInline` arrange
continuations without
blocking the event thread.
An `OnReady` callback may run immediately during registration or on the
completing thread, so marshal UI changes to the owning event thread. `Cancel`
is a request; retain native request buffers and handles until their completion
drains. Dropping an unfinished TaskGroup requests cancellation but does not
wait; observe the Task returned by `Finish()` before releasing child resources.
This profile carries `absl::StatusOr<T>` results and `absl::Status` failures
directly. Absent `Await` and fiber APIs still mark it as a bounded profile,
not the final A11 guest ABI.

The SDK should own tool selection, E32 conversion/relocations, runtime startup,
heap/TLS setup, tested leave/Status adapters, encoding helpers, cancellation-aware
request owners, handle RAII, service/path adapters, registration/packaging and
debug symbol relocation. A developer should still specify identity/permissions,
choose encodings and allocation policy, retain asynchronous ownership, manage
application lifecycle/durable state and test the required OS services.

A workaround mentioned here is not a permanent demand on every application.
When an adapter closes a gap, update this guide to describe its public contract
and remove the manual workaround. Track unsupported runtime capabilities in
RUNTIME.md/STATUS.md; keep this guide focused on writing a robust application.
