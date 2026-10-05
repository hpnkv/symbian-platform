# Write a C++ application for Symbian

Device selection is independent of application and SDK artifacts. See
[ROM/Z configuration and transfer](../guides/firmware.md). The SDK resolves settings,
verifies firmware and owns fresh emulator copies; application code does not need
ROM paths, firmware-specific executive maps or display-scale calculations.
The modern application runtime requires EKA2 services. EKA1 uses the separate
[legacy process profile](../guides/eka1.md), which does not support GUI starters.

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

Choose facilities from the [runtime profiles](runtime.md) and
[concurrency APIs](concurrency.md). Guest JSON, general C++ TLS, thread-safe
local statics and thrown C++ exceptions are unsupported. A host library's
availability does not make it available in the guest.

## Keep application logic away from the platform ABI

The application executes as 32-bit ARM code. Pointer and integer sizes differ
from a modern desktop host; use fixed-width types for persisted/wire data and
check ranges before passing sizes to APIs taking signed TInt. Never serialize a
pointer or copy a host-built C++ object's memory layout into a guest structure.
A host GTest passing on a 64-bit Mac cannot establish a 32-bit SDK layout.

System DLLs have fixed calling conventions, object layouts and numbered exports.
New compiler/STL objects are suitable inside your own consistently built code;
they are not a replacement ABI for existing OS DLLs. Pass the platform's expected
descriptor/handle or a narrow C interface. Do not expose
std::string, std::vector, Abseil maps or nlohmann objects across a frozen system
DLL interface, and do not assume two separately built runtime configurations are
interchangeable. [The image contract](https://github.com/SymbianSource/oss.FCL.sf.os.kernelhwsrv/blob/0c3208650587ac0230aed8a74e9bddb5288023eb/userlibandfileserver/fileserver/inc/f32image.h)
is independent of the host compiler.

The SDK selects the triple, headers, runtime configuration and import
proxies, and rejects unsupported binaries with diagnostics. Application
code should never embed syscall numbers or copy functions from the emulator.
The current runtime's SDK adapter also isolates a real placement-new header
conflict; see the [runtime guide](runtime.md). Do not work around that conflict by editing OS headers
or disabling exception-specification checks in your application.

## Treat descriptors as views or buffers with an explicit encoding

Application-menu captions are configured in `symbian.toml` with fallback
text and optional BCP 47 keyed translations. An SVG icon is a
project-relative asset. The SDK compiles these to native resources and a
MIF container; application C++ does not need RSS or SIS language syntax.
This is menu localization only. In-app text still needs an owned UTF-8
model and an explicit conversion for UI calls.

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

Your application must choose its encoding, check capacity and retain ownership
until completion. Qt helpers follow the same ownership rule: use QString/QJson
only where an external Qt API actually requires them.

## Distinguish returned errors, leaves and panics

These are different control-flow contracts:

| Result | Application responsibility |
| --- | --- |
| Returned TInt / completed request error | Check it and translate into the application's status policy at the adapter |
| A leaving API, usually suffixed `L` or `LC` | Call only through a leave/cleanup adapter |
| Panic | Treat as a failed invariant/programming contract and retain the diagnostic |

A leave is not automatically an Abseil Status and is not a portable C++
exception. An `LC` function additionally transfers a cleanup-stack obligation.
Do not assume std::unique_ptr destructors or a C++ catch block implement raw
Symbian leave handling. The preserved [cleanup and trap interfaces](https://github.com/SymbianSource/oss.FCL.sf.os.kernelhwsrv/blob/0c3208650587ac0230aed8a74e9bddb5288023eb/kernel/eka/include/e32base.h)
must be respected by the adapter.

The SDK's host native libraries disable C++ exceptions and return
Status/StatusOr. Guest applications currently compile with exceptions off by
default. A guest exception profile is not available.
Imported typeinfo and general throw/catch unwinding are unsupported. A Symbian
leave remains a separate contract; use an adapter that traps and translates
it. Calling an `L` function without its required leave handling is unsafe.

Allocation policy also matters. The initial guest libc++ profile makes ordinary
new terminate the guest with KErrNoMemory; nothrow allocation returns nullptr.
A container's allocation failure therefore does not currently become a
recoverable StatusOr. Bound input sizes and working sets. If your application
must recover from memory pressure, require a tested fallible allocation API
before choosing that container for the operation. The [runtime guide](runtime.md)
describes the allocation policy. `std::nothrow` works in the guest profile;
the SDK relocates its local GOT entry. Generated model creation checks null
and releases acquired native resources. This fallible acquisition does not
make subsequent string/vector growth recoverable.

## Static storage and lifecycle

New projects use the SDK's independent code/data linker layout.
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
C++ globals may own runtime objects such as `std::string`.
Thread-safe local-static guards, TLS and general DLL teardown are unsupported.
Unsupported sections and relocation forms are errors, not silently removed.
See the [runtime guide](runtime.md) for supported storage and failure behavior.

The SDK preserves Clang's ARM variadic-call rules with OpenC headers and
supplies libc++ error categories. Formatting and error messages require compatible
`vsnprintf` and `strerror_r` exports from the selected firmware's `libc.dll`.
Use `Symbian::Streams` for classic C/POSIX locale and `std::ostringstream`,
instead of `Symbian::Runtime`; file streams, general wide I/O and other named
locales are unsupported. Abseil-based components select the matching streams
runtime. Imported function pointers are supported; imported data objects,
including exception typeinfo, are unsupported.

DLL `.data` and `.bss` receive per-process storage and relocation. SDK DLL
startup runs process-attach constructors, and `RLibrary::Close` releases a
dynamically loaded DLL. Keep mutable globals default-visible where the compiler
must emit GOT references; hidden cross-mapping references fail conversion.
Applications need no historical `EPOCALLOWDLLDATA` MMP directive. Declare
compatible import proxies and check that the selected firmware supplies them.

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
describe these OS contracts.

Keep the UI/event loop responsive. Long computation, filesystem work or a wait
inside the dispatcher delays unrelated events and redraws. Raw active objects
need a scheduler and define their RunL/error/cancellation behavior; ordinary
C++ code does not acquire those semantics by declaring a callback. Use the SDK's
[event executor](concurrency.md) to coordinate timers, property watches,
mailbox callbacks and fibers with one native wait. Window Server requests
remain owned by the application. Use `WorkerExecutor` for blocking I/O and
computation; the event thread should only dispatch bounded work.

For composition, link `Symbian::Stackless` and use `Promise`, `Future`, `Task`,
`Then`, `JoinAll` and `TaskGroup`. A producer completes its Promise from a
native completion or worker. `OnReady` may run inline, even during registration.
Marshal UI work to its owning event thread. Observe the Task from
`TaskGroup::Finish()` before releasing child resources; abandoning a group
requests cancellation without waiting for drainage.

Link `Symbian::Fibers` when a guest operation needs cooperative `Await`.
An unresolved `Await` outside a fiber returns an error. Keep each fiber and
its scheduler alive until it finishes. Blocking OS calls still block its whole
OS thread; fibers do not virtualize native heap, TLS or leave/TRAP state.

Use `absl::Time` for absolute real-world deadlines and `absl::Duration` for
relative delays. `TimerPump` converts an accepted absolute deadline once to
monotonic waiting; later wall-clock corrections do not move it. Timer admission
is bounded to 64 by default; scheduling at capacity produces an explicit
resource-exhausted result. Observe the result even if scheduling completes
immediately. See the [runtime guide](runtime.md) for clock and atomic profiles.

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
jumping into a C++ function. SDK startup supplies the secondary thread's heap
and entry path. General TLS cleanup and DLL teardown ordering are unsupported;
keep service state explicitly owned.
The [heap/session interfaces](https://github.com/SymbianSource/oss.FCL.sf.os.kernelhwsrv/blob/0c3208650587ac0230aed8a74e9bddb5288023eb/kernel/eka/include/e32cmn.h)
make these distinctions explicit.

Prefer explicit lifetime for application services. A custom ELF section does
not supply missing lifecycle behavior. Keep mutable service state in its owner
and make shutdown drain outstanding work before releasing resources.

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
launch-menu visibility and starting an executable by path are separate operations.
A Window Server client drawing a window is not automatically a fully registered
application with lifecycle integration. The SDK now owns menu registration
resources, translated captions and icon packaging. The Window Server starter
does not implement an Avkon application lifecycle. You must still select a stable application
identity and permission policy.

Keep executable code and writable application data separate. Symbian paths are
drive-based; Z is the ROM view, system executables use `sys/bin`, and private
application data belongs in the appropriate security-controlled location.
Do not hard-code C as the only writable volume or depend on a desktop working
directory. Removable/absent/full storage and failed writes are normal conditions.
The application chooses which
data is durable, private, exportable or disposable. The [file-server interface](https://github.com/SymbianSource/oss.FCL.sf.os.kernelhwsrv/blob/0c3208650587ac0230aed8a74e9bddb5288023eb/userlibandfileserver/fileserver/inc/f32file.h)
defines the native drive and permission model.

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

## Debug and test the application

Test portable model behavior on the host and exercise input, rendering,
allocation failure, cancellation and shutdown in the emulator. Check the native
guest exit or panic record as well as the frontend's exit code.

Keep the debug ELF paired with the executable installed for the session.
Runtime load addresses can differ from link addresses; let the SDK relocate
symbols. Rebuild and publish a fresh ELF/E32 pair after source changes. See
[CLion Run and Debug](../guides/clion-run-debug.md).
