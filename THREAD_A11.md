# A11 `thread::` parity on Symbian

Implementation handoff, 2026-10-02. This is the concurrency workstream in
[PLAN.md](PLAN.md), not a claim that the guest already has parity. The pinned
A11 source and original tests are under
`cpp/symbian/concurrency/upstream/` (revision
`fcccb8cb6e1e67d7ba0822ac14cece9ee4c7091b`); verify their digests with
`scripts/check_a11_concurrency.py` before adapting them. Preserve the original
license and paths. Read [docs/RUNTIME.md](docs/RUNTIME.md),
[docs/STATUS.md](docs/STATUS.md) and the newest entries in
[docs/RESEARCH_LOG.md](docs/RESEARCH_LOG.md) before changing the backend.
Work as one agent; do not spawn subagents or operate on a physical phone.
Inspect git status and diffs before editing: existing uncommitted work and
user-owned application sources are intentional and must be preserved.

## Target contract

An application should use `thread::Mutex`, `thread::MutexLock`,
`thread::CondVar`, `thread::Channel<T>`, `thread::PermanentEvent`,
`thread::SelectUntil`, `thread::Fiber`, `thread::SleepFor` and the A11
Future/Task/structured-work APIs with the same observable semantics on hosts
and guests. Keep `thread::` and `a11::` as the public namespaces, with no type
aliases invented merely to abbreviate Abseil names. `Task` remains
`Future<Unit>`; `OnReady` may invoke inline; `Post`/`PostAt` use the shared
pool; `Submit`/`Schedule` use fibers. A custom scheduler/policy must control
ready-fiber selection and park/wake integration without changing those API
meanings. `absl::Time` is an absolute wall-clock deadline and
`absl::Duration` is a relative delay. Convert accepted deadlines once to a
verified monotonic wait; use that clock for elapsed waits and native timer
rearming. `absl::Now()` must remain wall time. Complete and execute the needed
guest Abseil time closure before exposing A11-compatible `Await`,
`SelectUntil` or `PostAt`. A ready Future wins over an already-expired Await
deadline, as in the pinned A11 implementation; a pending one times out without
overwriting its producer's result. Define infinite past/future, finite past,
negative relative delay, unrepresentably distant finite time and clock-source
failure separately. Do not invent an invalid `absl::Time` sentinel; validate
malformed external inputs at their parsing boundary. Document a separate API
if an alarm must follow later wall-clock changes.

Parity means common source-level contracts, cancellation/completion behavior,
ownership, diagnostics and tests. It does **not** require Boost on the guest,
identical throughput, identical native handles or moving a live fiber between
Symbian OS threads. A11's host implementation may use Boost privately. Host
SDK distributions must expose Boost-free headers and include its object code
in SDK archives; consumers need Boost only to rebuild the library. Native
libraries default to `-fno-exceptions`, while selected translation units may
enable exceptions as A11 does. Do not silently use Boost forced unwind in a
guest no-exceptions fiber or swallow it on the host.

Current guest implementation is intentionally narrower: `guest/thread/fiber.*`
provides a manually pumped, one-OS-thread scheduler and 16 KiB heap stacks;
`guest/thread/boost_primitives.h` parks fibers for mutex, condition and sleep
operations. Guest `thread::Channel` now has selectable cases and zero-capacity
rendezvous with A11's public signatures; SDK mailboxes use a distinct fallible
`symbian::concurrency::BoundedChannel`. TimerPump, PropertyWatch and EventMailbox own
some native requests and callbacks, but are not one general A11 executor.
The guest fiber probe passed ARMv5T/ARMv6 × Dyncom/Dynarmic normal and changed
controls; it did **not** run the original A11 tests. The pinned A11 host
`cpp/thread` compiled and passed its original thread, introspection and
affinity test executables in `full_host_probe/`, but the staged host archive
still implements only a subset. Maintain this distinction in status reports.

## Compatibility inventory and acceptance tests

Port A11's original tests as the behavioral oracle. Keep a mapping from each
test to a guest equivalent, with deliberate host-only cases labeled and
replaced by an equivalent invariant. Do not mark a feature done for compiling
its header or passing one happy-path probe. Begin with the pinned
`thread/tests/thread_test.cc`, `fiber_introspect_test.cc`,
`thread_affinity_test.cc` and `cpp/tests/future_test.cc`; add A11 concurrency
tests outside that snapshot where their current source proves a contract.

| Contract | Present guest evidence | Required parity gate |
| --- | --- | --- |
| Mutex, condition, sleep | Cooperative contention, timeout, worker signal | Original handoff/timeout/yield cases, races, lock ownership, no blocked event OS thread |
| Channel | Selectable buffered/rendezvous transfers, losing-case value preservation, timeout and direct cancellation in guest matrix; host A11 header test | Wider race stress, waiter drainage and original A11 test mapping |
| Selection/events | Guest `Case`, `PermanentEvent`, `Select`/`SelectUntil`; installed-SDK immediate, timeout, worker notification and competing-event controls pass | Channel and cancellation cases, simultaneous readiness, timeout/cancel unregister, no stale waiter under race stress |
| Fiber lifecycle | Yield/sleep/finish | Root/child trees, inherited cancellation, join/detach/reap, move-only captures, thread-exit placeholders, deterministic cleanup |
| Shared execution | Explicitly pumped event scheduler only | `Post`/`PostAt` stackless pool, `Submit`/`Schedule` fiber pool, bounded shutdown, idle wake and worker failure behavior |
| Structured futures | Bounded staged `symbian::concurrency` profile | Original `a11::Future`/`Promise`/`Task`, `Await`, `Then`, `AwaitAll`, `ThenAfterWaiting`, inline/reentrant completion, abandonment, cancellation ownership |
| Structured owner | Timer-backed `TaskGroup` with cancel forwarding and asynchronous `Finish` | Inherited cancellation/deadline, child registration before start, owner-close cancel and asynchronous join **after** native request drainage, child errors and repeated close |
| Observability | Basic guest state | Waiting reason, parent/child, mutex holder, cancellation and join state, safe snapshots and debugger-visible stacks |
| Python on hosts | Bounded wheel interop controls | A11 `PythonLoop` capture/resolve, asyncio completion/cancel, park guard, deferred refs, loop close and interpreter shutdown tests |

The `thread_affinity_test` CPU pinning cases are Linux-host-specific. Guest
equivalents must instead assert OS-thread affinity, heap/TLS identity and that
live fibers never migrate. Do not invent an affinity result on a target that
cannot report CPUs. A11's current test for migrating stacks needs an explicit
guest adaptation: steal **unstarted work**, not an already-running stack,
until cross-thread fiber migration and its allocator/TLS implications have
separate proof. Record that backend difference while preserving functional
`Submit`/`Schedule` behavior.

The guest C++ allocation bridge now records and pins the creating `RHeap` per
allocation, so a consumer may free directly across private thread heaps while
the producer is alive or after it exits. The `RChunk` page owner likewise pins
the heap holding its metadata. This is a lifetime and correctness gate, not a
fiber-migration result. An optional disconnected-`RChunk` mimalloc v3.5.3
backend now passes the private-heap and bounded reuse guest controls. A
bounded native per-thread cache eliminates its measured OpenC pthread lookup
on explicitly managed threads; the warmed emulator burst is faster than the
original heap on both CPU backends. Raw native threads stay on pthread TLS
unless their owner pairs cache entry and exit. The standard and streams SDK
archives now select mimalloc by default, with `SYMBIAN_RUNTIME_MIMALLOC=OFF`
retaining the original heap. Phone latency and memory-pressure evidence remain
open under the `PLAN.md` allocator gates.

## First executable vertical slice

The next session should deliver this slice before expanding the whole API
surface or only writing more design. Reuse the existing guest Abseil
Status/StatusOr/time profile, TimerPump, PropertyWatch, EventMailbox,
TaskGroup and bounded fiber backend; replace the overlapping event wait and
dispatch mechanisms with one owned event executor. Its first two request
adapters are `RTimer` and the already implemented `RProperty::Subscribe`.
The property result is a real OS-service value, not a synthetic completion.
Run both through one event-thread request-semaphore consumer, and dispatch
their results, mailbox callbacks, Window Server events and ready fibers in
bounded turns. The service and timer adapters must own their statuses,
buffers, handles, cancellation and final result through drainage. Make an
explicit event-affinity dispatch operation; do not repurpose `thread::Post`
for it. Preserve the guest EventMailbox. Its fallible nonblocking queue is
now a separate `symbian::concurrency::BoundedChannel<T>`; A11's selectable
`thread::Channel<T>` keeps its original public signatures and close semantics.
No second scheduler or native request-semaphore consumer is allowed.

Add a structured owner that starts a timer task and a property-watch task,
passes its cancellation and absolute deadline to both, and returns an
asynchronous join task. Closing the owner cancels both children, drains native
requests, then completes the join exactly once. A fiber may `Await` that join
without blocking the OS event thread; an unresolved Await on that event
thread outside a fiber returns a clear failed-precondition error. Test a
property change, timer expiry, cancellation before/after native completion,
immediate completion, error, abandoned producer, reentrant `OnReady` callback,
timeout, repeated close and resource balance. Run normal and changed-result
controls on ARMv5T/ARMv6 × Dynarmic/Dyncom, including a forced one-worker
profile. Integrate the verified path into `examples/gui_app` first and then
the `symbian init` starter, keeping native status and bridge types inside the
SDK. Only after this slice passes should the broader A11 Select/tree/pool
work advance. This slice is not, by itself, full A11 parity.

## Architecture and work order

### Event-thread work on one core

The dispatcher runs native completion handling, inline `OnReady`
continuations, event mailbox callbacks and ready guest fibers on its existing
OS thread. Their work is currently unrestricted: any one callback or fiber
can be compute-bound and hold the event thread indefinitely. A fiber switch
stays within that thread; a timer or property completion can therefore resume
an event-affine continuation in the same dispatch turn without handing it to a
worker. `Post`/`PostAt` still mean the
shared worker pool once the A11 guest pool is implemented. The guest now has
an explicit, one-worker `WorkerExecutor`: `EventExecutor::workers()` creates
it lazily, `future.ThenOnWorker(event_executor, transform)` chooses that
worker and keeps the transform stackless, and `worker.PostFiber(work)` starts
a fiber on the worker's own scheduler. Compute-heavy work should be submitted
explicitly; it must never be inferred from a callback and silently moved, because that
would change affinity and ordering. On a one-core device, that explicit
worker handoff costs an OS context switch. Keep the pool bounded, batch its
work and coalesce wakes so the switch buys useful compute time.

Bound each event source's work per turn, check native statuses before parking,
and resignal when work remains. A callback or fiber runs cooperatively; the
dispatcher cannot preempt arbitrary C++ code. Therefore an application that
performs unbounded compute in an event callback can still delay input and
native completion. The present per-source count budget does not bound time
spent inside one callback. Measure this with an emulator control that
saturates one worker while timer, property and Window Server events arrive; assert bounded
latency and progress, and verify event-local continuations stay on the event
OS thread. Track dispatcher turns, runnable depth and worker handoffs before
claiming a one-core performance policy. Avoid per-yield allocations and
callbacks under synchronization locks. Channel selection, timer wake and
future completion should publish state once, wake once and let the dispatcher
drain several ready tasks per park cycle.

`Then` and `Future::OnReady` retain pinned A11 inline behavior.
`ThenOnWorker` is an opt-in guest extension; its readiness callback enqueues only a Future
handle, while result copying and transformation happen on the worker. A
full or closed worker completes the returned Future with an error. The worker
caps queued and active work together. Its `Close` requests asynchronous
drainage and `Finish` exposes a Task; a fiber that waits forever can keep
that task pending because guest cancellation trees are still absent. This
does not provide a preemption guarantee or complete A11 pool semantics.

### 0. Freeze one semantic baseline

Run the source-pin check and all original host tests; record per-test results,
not only CTest's three executable totals. Inspect current implementation,
installed-SDK state and retained execution evidence first; historical
checkpoints in docs are not automatically current. Build a contract matrix
from every public pinned A11 header and original test. Inspect existing `symbian::`
callers and decide one mechanical migration into `a11::`/`thread::`; avoid a
second persistent API or scheduler. The isolated full-host probe proves the
selected exception boundary, not SDK distribution. Replace the staged host
implementation with the complete pinned A11 host implementation, adapt only
where needed, and bundle private Boost objects. Test an installed macOS and
Linux consumer with Boost discovery disabled, public-header compilation, and
actual pool/fiber/Select/Python behavior. Link failure when mixing old and new
`thread::` objects should be made impossible by CMake target ownership, not
left to whichever archive the linker picks.

### 1. Make the Symbian low-level fiber and memory contract safe

Audit the ARM/Thumb swap in `guest/arm_fiber_context.S` against AAPCS, Thumb
interworking, unwinding, floating-point/coprocessor state, stack alignment and
the guest exception profile. Execute callee-saved-register, nested C++ RAII,
FP, return, cancellation and forced stack-pressure controls on ARMv5T and
ARMv6 with both emulator backends. Add bounded stack sizes, overflow detection
or a verified guard mechanism, ownership accounting and failure reporting.
Investigate `RChunk` for page-backed stack allocation/commit/release; a guard
page and cheap sparse commitment are hypotheses until actual EKA2L1 and ROM
execution prove them. Do not assume A11's host 512 KiB virtual-stack policy
has the same physical cost on a phone. Never switch while a native leave/TRAP
or cleanup-stack scope is active unless an explicit scope-state adapter has
been implemented and tested. Keep live fibers pinned to the creating OS
thread; verify per-thread heap, handles and TLS at every resume.

### 2. Unify the event executor and native request ownership

Create one event-thread completion owner rather than independent timer,
property, Window Server and scheduler waits. It owns every submitted
`TRequestStatus`, request buffer, handle and cancellation/drain state. Register
ownership before submission; handle immediate completion; remove a completed
entry before invoking a possibly inline callback; publish each result once;
retain resources until cancel and completion have drained. The event thread
alone consumes its request semaphore, dispatches all ready Window Server and
SDK statuses, then parks through `User::WaitForAnyRequest()` only after a
lost-wakeup-safe recheck. Cross-thread `RThread::RequestSignal()` calls are
coalesced. Bound each completion, callback and ready-fiber turn; resignal when
work remains, so UI latency does not depend on a long queue. Integrate
`Scheduler::RunReady` and `NextDeadline` with this owner, rather than adding a
second sleeper. Provide an explicit event-thread dispatch API. If a legacy
`CActiveScheduler` or Active Object must coexist, integrate it under the same
request-semaphore owner and prove its statuses drain; never let two schedulers
compete to consume a completion. Refactor TimerPump, PropertyWatch and
EventMailbox behind it without changing their tested cancellation semantics.
Add at least one more
real service request **after** the timer/property vertical slice before calling
the owner general; choose a service with
observable result and cancellation, then test immediate completion, late
completion, cancel, close and resubscription in a callback.

### 3. Port selectable synchronization and structured fiber ownership

Port A11's `cases.h`, `selectables.*`, `select.*`, channel waiter state and
`PermanentEvent` as one state machine. `thread::Mutex`/`CondVar` must park a
fiber when one is active; they may block an ordinary worker OS thread, but
must never block the UI/event OS thread inside a fiber. Wait registration,
notification, timeout and cancellation must have a single winner and remove
all losing cases. No callback or user work may run under an internal lock.
Resolve the current channel API mismatch explicitly: the staged guest
`Writer::Write` returns `absl::Status`, while pinned A11 declares a `void`
`Write` and uses a fatal check on closed writes (despite older header prose
describing an exception). Use the pinned implementation and tests as the
behavioral oracle, then choose and document any additional fallible guest
operation under a distinct name; do not silently call both APIs compatible.
Keep lock-owner diagnostics and test reentrant notification. Then port
`Fiber` trees: cancellation inheritance, child tracking, joinable event,
detached/reap ownership, worker-thread exit, and destructor invariants.
Cancellation is a request, not a guarantee that an unsafe native call was
interrupted; shutdown awaits native drainage and joins asynchronously when on
the event thread. Every child started under a structured owner inherits its
effective absolute deadline and cancellation source; a child may narrow a
deadline but not silently outlive its owner. Register it before native submit.
Owner destruction/close requests cancellation and retains owned state until
all native handles, statuses and buffers have drained; `Finish` exposes the
asynchronous join without blocking the UI thread. Define which child error
is reported when several fail, and test completion/cancellation races,
reentrant callbacks, abandonment and shutdown. Ensure C++ local destructors
run normally. Do not use a
Boost-like forced unwind shortcut in default guest builds.

### 4. Complete the A11 execution and future APIs

Port the actual `a11/concurrency` headers and implementations, preserving
`OnReady` inline execution, `Task = Future<Unit>`, cancellation hooks,
producer abandonment, Status/StatusOr propagation, `Await` deadlines and
`CallbackScheduler` progress semantics. A pending `Await` on the event thread
outside a supported fiber must fail clearly; in a fiber it parks just that
fiber. `Then` stays stackless where possible. `Post`/`PostAt` target a shared
worker pool, never silently the UI thread. `Submit`/`Schedule` create fibers
with structured ownership and reaping. `CallbackScheduler` currently needs
more than one turn in flight; design and test a serialized event-executor
adapter rather than setting its limit to one. Use `absl::Time` for absolute
deadlines and `absl::Duration` for relative delays, including `Await`,
`SelectUntil` and `PostAt`. Preserve wall-time semantics at registration and
monotonic waiting afterward. Define past, infinite and very distant values;
reject malformed external deadline input at its boundary. Split waits that
exceed `RTimer`'s signed relative range; do not truncate them. Test wall-clock
changes, 32-bit tick wrap, long waits, timer range and missing clock support.

Start the guest pool with one or a small bounded number of verified
`RThread`/`std::thread` workers and their own heap/TLS startup and shutdown.
Treat a reported hardware concurrency of zero as unknown and provide a
configurable, bounded fallback; do not equate ARMv5T/ARMv6 with a core count
or trust an emulator's host CPU count as the guest's. The initial single-core
profile should have one pool worker alongside the UI/event thread, with the
number of workers independently configurable for workloads that need more
OS threads. Test the one-worker path as a first-class configuration.
Prove cross-thread allocation/free, Abseil state, handle ownership, DLL
lifetime and thread-exit destructors before adding work stealing. Steal only
unstarted jobs initially. Idle workers should sleep using a native primitive,
not poll; waking one worker should not wake all. Expose capacity and failure
through SDK configuration and `absl::Status`, and measure whether more workers
help on the available devices. Preserve A11's user-facing pool semantics even
when the guest deliberately chooses fewer threads or a pinned-fiber backend.

### 5. Complete host Python and developer integration

Port A11's real `cpp/python/interop.*` loop-capture/resolve and cancellation
behavior in the pybind boundary, with one selected exception-enabled TU where
needed. Release the GIL around native work and scheduler parks; acquire it
before Python access. `DeferredPythonRefs` destructors only retire references;
drain with the GIL held under the documented shutdown contract. Test inline
completion, worker completion, Python cancellation, loop close, interpreter
shutdown, callback exception translation and ref balance. Python is a host
feature; no Python runtime is required on the phone.

Migrate `examples/gui_app` first, then the `symbian init` starter, preserving
user-owned application edits and UIDs. Show a task group, future continuation,
fiber await, channel/message transfer and cancellation on close, with UI
updates dispatched to the event thread. Ordinary examples should not expose
`CActive`, `TRequestStatus`, `void*` or `extern "C"`; keep such bridges inside
the SDK. Provide a small SDK guide for choosing task/future/fiber/thread work
and lifecycle cleanup. Refresh the visible SDK deliberately and test projects
against installed archives on both ARM architectures.

## Single-core and one-worker behavior

Many target phones may have one runnable CPU even though the application has
an event OS thread, a pool worker and many fibers. Keep four quantities
separate: available cores, pool OS threads, active callback turns and runnable
fibers. None should be inferred blindly from another. A11's host pool already
falls back to one worker when `std::thread::hardware_concurrency()` reports
zero. Its `CallbackScheduler` enforces at least **two turns in flight**; that
is a progress rule for its continuation queue, not a requirement for two CPU
cores or two workers. Preserve it on the guest. Verify that both turns can
advance cooperatively on one worker, and provide a separately tested event
executor adaptation where needed. Never reduce the turn count to one merely
because hardware concurrency is one.

On one core, a runnable fiber that spins, a callback that blocks an OS thread,
or an unbounded ready queue can starve every other logical task despite
several registered fibers. `Post` callbacks must remain short and nonblocking;
`Submit` work may park on SDK futures/selectables and release the worker for
another fiber. For genuinely blocking native calls with no asynchronous form,
either supply an owned asynchronous adapter or route them to a bounded,
explicit blocking lane. Do not execute them on the UI event thread, and do
not pretend a second fiber on the same blocked worker will make progress.
Bound work per scheduler turn and give the event thread regular opportunities
to drain native completions and redraw. Use fair or aging-ready selection as
the safe default; custom policies must document starvation behavior.

Single-core efficiency favors a small initial worker pool, in-process queues,
coalesced wakeups and parking idle OS threads. Extra workers can still be
useful for blocking services, but add stacks, heap/TLS state, context switches
and battery wakeups without adding CPU capacity. Avoid long spin locks,
busy-polling and wake-all on a single core; after a short measured fast path,
park the waiter. `RFastLock` is suitable only for a brief non-yielding critical
section, not for protecting a callback or fiber switch. Measure one versus
two versus several workers before choosing a default on each device class.

Add a **forced one-worker** host and guest test profile independent of the
emulator host's CPU count. Run nested `Post`/`PostAt`, `Submit` awaiting another
submitted future, two active callback turns, channel rendezvous, mutex and
condition contention, timer completion, cancellation during a parked await,
shutdown/reap and reentrant inline completion. Set short watchdog deadlines
and retain traces on deadlock. Repeat while the UI receives pointer events;
assert bounded pointer-to-redraw latency and no lost native wake. Also test
`hardware_concurrency()==0` and a configured worker count greater than the
available cores. These are correctness tests first; compare allocations,
committed stacks, OS context switches, idle wakeups and energy proxies after
they pass. Record separately whether any A11 test genuinely requires an
additional OS worker because it performs blocking work, and adapt that path
explicitly rather than weakening the test.

## Native-interface experiments and performance decisions

These are candidates, not instructions to use every kernel facility. Inspect
the original `research/upstream/kernelhwsrv/kernel/eka/include/e32std.h` and
`.../euser/us_exec.cpp`, then verify exact imports and behavior against at
least one named ROM before selecting a backend.

| Candidate | Useful role | Risk and required measurement |
| --- | --- | --- |
| EUSER 32-bit atomics, ARMv6 exclusives, SDK 64-bit lock bridge | Ready-queue indices, ref counts, one-shot state transitions | Confirm memory ordering and lock-free claims per ARM profile and ROM; measure CAS retries. Do not assume 64-bit lock-free. |
| `RFastLock` | Brief cross-OS-thread shared-state critical sections | Fast uncontended path is promising, but contended `Wait` blocks an OS thread. Never use it as the fiber park primitive or hold it across callbacks/switches/native leaves. Compare with tested libc++ mutex. |
| `RMutex`/`RCondVar` or `RSemaphore` | Park idle **worker OS threads** or protect rare shared state | Kernel handles, scheduling cost and timeout precision versus libc++/pthread; do not block the UI fiber scheduler. Prefer one worker wake over broadcasts. |
| `RThread::RequestSignal` and the thread request semaphore | Cross-thread event-loop wake after coalescing | One consumer only; inspect all owned statuses after wake and preserve unrelated Window Server completions. Measure idle wake count and stale signals. |
| `RTimer::HighRes` | One earliest-deadline timer for many fiber waits | Re-arming and cancellation/drain costs, range segmentation, races and battery wakeups. Compare one timer heap with one native timer per task; never use `User::AfterHighRes` on the event thread. |
| `User::NTickCount` plus HAL period | Production monotonic elapsed time | 32-bit wrap, period accuracy and deadline extension on each ROM. `User::FastCounter` with HAL frequency is a **profiling** candidate, not an assumed stable production clock. |
| `RChunk` | Bounded fiber stacks, optional page backing/guards | Commit and resident-memory behavior, guard support, fragmentation, handle count and same-owner free. Keep heap stacks if the measured native route is worse or unsafe. |
| `RMsgQueue`/`RProperty` | Optional cross-thread/process notification or observable service | Kernel copy/handle and wake cost against the in-process A11 MPMC queue plus one request signal. Use only if it improves a measured path or provides needed IPC semantics. |
| `RThread::Logon`, per-thread heap/TLS APIs | Worker exit, join and cleanup | Drain worker work before handle close; prove thread-relative heap/TLS and native status ownership across startup/exit. |

Build a benchmark harness with raw native, stackless and fiber paths. Record
per-operation allocations, heap cells and committed stack bytes; idle wakeups,
context switches, ready-to-run latency, timer jitter, UI pointer-to-redraw
latency, cancellation drainage time and battery/CPU proxy where measurable.
Run uncontended and contended locks, N-to-1 queue transfers, 1/64/1024 timers,
short and long tasks, cancellation storms and shutdown. Include forced
one-worker and oversubscribed-worker runs even on a multicore host. Report ROM identifier,
architecture, Dynarmic/Dyncom, compiler and build configuration, repetitions,
distribution (not just a mean), and a changed-result control. Do not describe
an emulator throughput win as physical-device performance. Update
[PERFORMANCE_CONSIDERATIONS.md](PERFORMANCE_CONSIDERATIONS.md) with findings.

## Release gate

For every phase, run meaningful GTest/CTest on hosts and source-built plus
**installed-SDK** guest tests on ARMv5T and ARMv6 with Dynarmic and Dyncom.
Exercise at least two suitable non-RM-807 ROM/Z profiles where the tested
system services are present; report missing-ROM capabilities as explicit
failures or skips, never silently emulate their result. For each async path,
include completion/cancel races, immediate completion, reentrant callback,
abandoned producer, timeout, shutdown and changed-result/negative controls.
Measure resource balance after repeated cycles and verify GUI responsiveness.
Retain failed emulator artifacts. Keep compiler success, emulator execution
and physical-device compatibility separate. No physical flashing or recovery
work is part of this plan.

Parity may be declared only after the contract matrix is complete, the
original A11 tests (or documented platform-equivalent cases) pass on both
backends, installed application examples execute and debug, and no competing
public scheduler remains. Update PLAN.md, docs/STATUS.md, docs/RESEARCH_LOG.md,
docs/RUNTIME.md, WALKTHROUGH.md and CXX_CAVEATS.md with exact evidence and
remaining platform limits.
