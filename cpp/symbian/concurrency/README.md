# Actual A11 concurrency sources

`upstream/` is an unmodified, licensed snapshot of A11's `cpp/thread` and
`cpp/a11/concurrency`, their complete quoted-include closure, original tests,
and source CMake declarations. Original repository-relative paths are retained.
`sources.json` pins every file and records include edges and external headers.
No files were generated to impersonate A11. The original working tree matched
these committed files at `fcccb8cb6e1e67d7ba0822ac14cece9ee4c7091b`.

```sh
cmake --build --preset debug --target symbian_a11_source_check
.venv/bin/python scripts/check_a11_concurrency.py --upstream ~/dev/a11
```

The verification target checks source identity and closure. The full pinned
host library can also be built and tested in isolation:

```sh
cmake -S cpp/symbian/concurrency/full_host_probe -B build/a11-host-probe -G Ninja
cmake --build build/a11-host-probe --target thread_test fiber_introspect_test thread_affinity_test
ctest --test-dir build/a11-host-probe --output-on-failure
```

This probe uses A11's original source and enables exceptions only in its
Boost-primitives and thread-pool translation units, matching A11. It does not
link against the staged SDK host library because both define `thread::`
symbols. The SDK adaptation is separately maintained in `common/`, `host/`
and `guest/`; the probe is not yet the SDK-distributed host archive.

The source includes the real Promise/Future/Task, inline continuation and pump,
CallbackScheduler, channel/select, cancellation/tree, introspection and pooled
fiber implementations. `OnReady` can execute inline; `Task` is `Future<Unit>`.
`Post`/`PostAt` go through the shared pool; `Submit`/`Schedule` use fibers.
CallbackScheduler's progress contract requires multiple turns in flight. This
cannot be converted into an event-thread executor by setting concurrency to one.

`common/` contains one explicit, licensed stackless adaptation of A11's
`future.h`, `parallel.h`, `inline_pump.h`, `task_group.h` and buffered
`thread/channel.h` at the recorded pin. Both host and guest compile those
same headers. `guest/thread/boost_primitives.h` supplies fiber-aware mutex,
condition and sleep operations over the ARM context switch; OS-thread callers
retain normal blocking behavior. `host/thread/boost_primitives.h` has an opaque
Boost-free ABI; only its implementation includes Boost headers. The host
`symbian::concurrency` CMake target links one bundled static archive containing
its primitive implementation and Boost.Fiber/Context object code. Ordinary
consumers neither include nor link separate Boost libraries. Boost is required
only to build this archive from source. The owned mutex shim is published as
`<symbian/concurrency/mutex.h>`.
The host also supplies a bounded `thread::Fiber` with move-only work, owner
thread affinity, cooperative cancellation, explicit join, normal C++ cleanup
and no detach/forced unwind. Its public header is Boost-free. The host now also
adapts A11's scheduler park guard, a selectable ready-fiber policy, its original
MPMC work queue, stackless shared-pool `Post`/`PostAt`, and the original
`PermanentEvent`/`Select` protocol with wait diagnostics omitted. The pool
coalesces OS wakeups while workers are busy. `cpp/python/concurrency_interop.*`
installs A11's CPython park guard and deferred-reference pattern; a bounded
`FutureToPython` bridge captures the running asyncio loop, releases the GIL
while registering native completion, then reacquires it and resolves on the
loop thread. Installed-wheel tests cover GIL progress, cancellation and
reference drainage.
This is still not A11's full fiber tree, pooled fiber work stealing,
`Submit`/`Schedule`, channel selection, introspection or shutdown contract.
The guest has its own
ARM-backed `thread::Fiber` and a separate `Symbian::Fibers` archive.
Its bounded `symbian::concurrency` API lives at
`<symbian/concurrency/*.h>`. The original `a11::` namespace and
`<a11/concurrency/*.h>` path are reserved for a compatible port of the
pinned A11 API. This API
provides Promise/Future/Task, inline OnReady/Then,
producer-driven cancellation, abandoned-promise completion and nonblocking,
ordered `JoinAll`, bounded reentrant `DriveInline` and an explicit
`TaskGroup::Finish` asynchronous join. The group forwards cancellation and
waits for every child before publishing the first error. Abandoning an
unfinished group requests cancellation; it cannot synchronously drain native
resources on the UI thread. Guest call sites use `thread::Mutex` and
`thread::MutexLock` explicitly. The guest backend uses the tested libc++
mutex for brief shared state and parks a contending fiber cooperatively. The SDK
exports these headers with `Symbian::Stackless`, which links the actual guest
Abseil StatusOr archive, matching streams runtime and selected OS proxies.
Future callbacks and results use `absl::StatusOr<T>` directly; failures use
`absl::Status`. This does not make the bounded API a drop-in copy of A11's
host ABI. Ready `Await` succeeds; unresolved guest `Await` parks inside an SDK
fiber and returns `FailedPrecondition` outside one. The Boost-backed
host profile parks an unresolved `Await` and honors its `absl::Time` deadline.
`TimerPump` and
`PropertyWatch` own the first two native request types. `EventExecutor` now
drives them, the bounded `EventMailbox` and the guest fiber scheduler in one
event thread, arming fiber deadlines through the same timer owner. Native
statuses still live in their existing adapters, and Window Server status
ownership remains with the application.
`NativeTaskOwner` provides the first structured timer/property pair with
inherited cancellation and an absolute deadline. Its asynchronous join waits
for child and deadline-alarm drainage. It is a bounded native-request owner,
not the complete A11 fiber tree or executor.

The guest also has an explicit `WorkerExecutor` for work known to be too long
for the event thread. `EventExecutor::workers()` lazily provides one shared
worker for that event owner. `future.ThenOnWorker(event_executor, transform)`
selects it; the lower-level `ThenOn(future, worker, transform)` keeps a
continuation stackless and performs its result copy and transformation on the
worker; `Then` and `OnReady` remain inline. `PostFiber` creates a fiber on
the worker's own `thread::Scheduler`. The configured cap covers queued and
active work, and a full or closed executor reports a status. `Close` does not
block the event thread; `Finish` reports asynchronous drainage. A fiber that
waits forever can prevent that drainage, and the guest still lacks the A11
shared `Post`/`PostAt` pool, cancellation tree and preemption.

The pinned full A11 **host** fiber backend can retain its selected-TU
exception boundary: Boost primitives and pool teardown use exceptions and
forced unwind, while the rest of the native library defaults to exceptions
disabled. A Symbian guest backend still needs an explicit adaptation for
cancellation, joining and destruction; its locks cannot be replaced by no-ops.
The pinned A11
source retains `thread::` and `a11::`; guest synchronization and channels now
also use `thread::`, while the staged Future/Task owner remains under
`symbian::concurrency`. No competing scheduler is introduced here.
The common `<thread/channel.h>` now carries the pinned A11 public
`thread::Channel<T>` interface on hosts; the guest adaptation has the same
reader/writer, selection, rendezvous, cancellation-aware write and one-time
close signatures. SDK mailboxes instead use the separately named
`symbian::concurrency::BoundedChannel<T>` for nonblocking status operations,
idempotent close and discard. `EventMailbox` uses that queue so enqueue cannot
block the event thread. The guest `thread::Mutex`, `MutexLock` and
`CondVar` are exposed by `<thread/boost_primitives.h>`. Fiber contention and
condition waits cooperatively switch to other ready fibers; timed waits use
the verified monotonic clock. `thread::SchedulerPolicy` supplies custom ready
ordering and a wake hook for the event executor. Guest `Case`,
`PermanentEvent`, `AlwaysSelectableCase`, `NonSelectableCase`, `Select` and
`SelectUntil` now use one selector and intrusive waiter registration protocol.
Notification chooses at most one case under the selector lock, unlinks event
waiters under the event lock, then wakes fibers after releasing both locks.
A shared selector lifetime prevents a late notifier from signaling a
destroyed condition variable. The guest rotates the first case rather than
using A11's random order. A finite wall deadline is converted once and
elapsed waiting uses a monotonic clock. The installed-SDK probe covers
immediate readiness, expired and timed selection, repeated timeout cleanup,
cross-thread notification and competing events on both ARM targets and both
emulator backends. The channel probe also covers buffered and zero-capacity
transfers, competing cases, losing-case ownership, timeout and direct
cancellation. Cancellation trees and joining remain open.
Guest `SleepFor` parks a fiber; outside one it blocks an OS worker and must
stay off the event thread. A11's channel closed-write check remains fatal in
the no-exceptions guest profile; callers needing fallible writes use the
separate SDK mailbox queue. This is not full host A11 compatibility.
Both backends match A11's `CondVar` boolean convention: true means timeout.
The guest condition-variable destructor comes from pinned LLVM libc++.
Normal and changed-result guest controls passed 16/16 across both ARM
architectures and both emulator CPU backends after an early timed-wait return
was handled using a signal generation and monotonic remaining time.
The first ARM/Thumb swap primitive ships in both runtime archives. A separate
`Symbian::Fibers` archive supplies the bounded pinned-fiber scheduler and
fiber-aware locks above the installed Abseil profile. Its normal/changed
guest controls pass 8/8 across ARMv5T/ARMv6 and Dyncom/Dynarmic. The event
owner still must integrate `RunReady` and `NextDeadline` with its single
native wait; full A11 scheduling, joining and cancellation are not claimed.

The manifest lists external includes; it is **not** a lock for the external
Boost/Abseil binary closure. Upstream dependency declarations select Abseil
20260526.0 and bootstrap Boost 1.90.0 (minimum find_package version 1.82).
The SDK's host Abseil is separately pinned. The installed guest Abseil profile
now executes bounded Status/StatusOr/Cord/map/time controls. General Abseil,
static lifetime, OS TLS and broader native completion remain gates.
Independent writable EXE data/BSS has now passed guest execution. It does not
establish these other contracts. See PLAN.md's C0–C5 gates.

A direct guest `std::make_shared<std::string>` build initially reached
libc++'s `__shared_weak_count` link dependency. The runtime now compiles the
original `memory.cpp` and pthread-backed thread sources with threads enabled.
Bounded shared/weak/unique ownership and one `std::thread` race path pass on
both ARM targets and emulator CPU backends, including worker destruction of a
moved `unique_ptr`. This does not establish A11 Promise/Future readiness.

The guest runtime now has a bounded 32-bit compiler-atomic bridge to the
original EUSER operations. It executes Clang's observed ARMv5T `__atomic_*`
and ARMv6 `__sync_*` calls for add, acquire load, compare-and-swap and exchange
on both emulator backends, with a changed-result control. A separate
`std::thread` probe checks 4,000 concurrent increments and a changed-result
control. Complete 64-bit atomic coverage and A11 shared Future state remain
unverified.

Guest exceptions are a planned opt-in profile; the SDK default remains
no-exceptions. Original Symbian tools require ARM unwind tables and a Symbian
exception descriptor in the E32 header. Current linker scripts discard those
tables, and `std::future` exposes missing exception ABI symbols at link.
Throw/catch and destructor-unwind execution controls must pass before this
profile or A11 exception-dependent components can be advertised.

A separate guest C bridge now executes the original EUSER `RFastLock` API on
both ARM client profiles and emulator CPU backends: uncontended `Poll`, a held
lock's `KErrTimedOut`, `Signal`, `Wait` and close. It is a prerequisite, not an
A11 `thread::Mutex` implementation. A11's mutex is fiber-aware: a contended
fiber must park through the A11 executor without blocking the only event OS
thread. The OS fast lock can guard true cross-thread shared state, while the
fiber backend needs a distinct wake/ownership route under the same A11 API.
An ARM AAPCS context switch must preserve registers and stack alignment and
stay pinned to its OS thread; it cannot virtualize OS TLS, heaps or leave/TRAP
state by changing SP. No context-switch backend is yet enabled.
