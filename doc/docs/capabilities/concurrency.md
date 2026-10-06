# Concurrency

The SDK follows [A11](https://github.com/hpnkv/a11)'s concurrency model and uses
its licensed thread primitives. Common Future/Task and channel headers are
shared by host and guest; each platform supplies its own primitive backend.
Use `<symbian/concurrency/...>` for completion and executor APIs, and
`<thread/...>` for fibers, synchronization and channels.

## Futures and Tasks

Link `Symbian::Stackless` in a guest application. `Promise<T>` produces a
`Future<T>` carrying `absl::StatusOr<T>`; `Task` is `Future<Unit>`.

- `OnReady` and `Then` run inline on the completing thread, or immediately
  when registered on a ready Future. Marshal UI changes to the event thread.
- `JoinAll` preserves result order. `DriveInline` bounds reentrant work.
- `Cancel()` requests producer action; it does not complete the Future itself.
  An abandoned incomplete Promise publishes a cancelled result.
- `TaskGroup::Finish()` completes after every child settles and reports the
  first error. Dropping an unfinished group requests cancellation without
  synchronously draining its native resources.
- An unresolved guest `Await` parks inside an SDK fiber and returns
  `FailedPrecondition` outside one. A ready Future can be read immediately.

Retain request buffers, statuses and handles until completion has drained.
Cancellation requested does not mean ownership can be released.

### Turn a timer into a reminder

Pass an already-open event-thread timer pump. The returned Future carries the
reminder text or the timer's error; it does not block the event loop.

```cpp
#include <string>

#include <absl/base/nullability.h>

#include "symbian/concurrency/timer_pump.h"

symbian::concurrency::Future<std::string> ReminderAfter(
    symbian::concurrency::TimerPump* absl_nonnull timers) {
  namespace tasks = symbian::concurrency;
  return tasks::Then(timers->ScheduleAfter(absl::Seconds(30)),
                     [](const absl::StatusOr<tasks::Unit>& result)
                         -> absl::StatusOr<std::string> {
                       if (!result.ok()) {
                         return result.status();
                       }
                       return std::string("Time to check the oven");
                     });
}
```

Keep pumping native completions while the reminder is pending. Retain the
Future to observe the result or request cancellation; the pump must stay alive.
An `OnReady` observer may run immediately when attached to a completed Future.

## Native request owners and the event loop

`NativeTimer` owns one thread-relative `RTimer`. Create, arm, cancel and close
it on the same OS thread. It rejects overlapping arms and drains a pending
request before close; the surrounding event loop owns the shared semaphore wait.

`TimerPump` turns timer completions into Tasks. `ScheduleAfter` accepts an
`absl::Duration`; `ScheduleAt` accepts an absolute `absl::Time` and converts it
once to monotonic waiting. Later wall-clock corrections do not move the request.
Negative delays fail with `InvalidArgument`; unrepresentable finite deadlines
fail with `OutOfRange`. Past deadlines complete on the next turn; infinite
future stays pending until cancellation or close. Long waits use native arms
of at most signed 32-bit microseconds.

The pump admits 64 pending timers and dispatches at most 64 completions per
turn by default. Saturation returns a ready Task with `ResourceExhausted`.
Workers may request cancellation; the event thread alone touches the timer and
publishes completion. `DispatchReady` may invoke callbacks inline.

`PropertyWatch` owns one integer Publish & Subscribe property and one change
request. `Next()` returns `Future<int>`. Open, subscribe, set, dispatch and
close on the event thread; worker cancellation coalesces a wakeup. The request
is removed before its callback, so that callback may subscribe again.

`EventMailbox` admits bounded cross-thread callbacks through a nonblocking
queue. Dispatch runs callbacks outside its lock and defers reentrant work to
a later turn. Its default admission limit is 128.

`EventExecutor` drives timer and property completions, the mailbox and guest
fibers on one event thread. It arms fiber deadlines through the same timer
owner. Window Server status ownership stays with the application: process
those completions before the single native request-semaphore wait.
`NativeTaskOwner` composes timer/property work with inherited cancellation and
an absolute deadline; its asynchronous join includes deadline-alarm drainage.

## Workers

Use `WorkerExecutor` for blocking I/O and long computation.
`EventExecutor::workers()` lazily creates one shared worker for that event
owner. `future.ThenOnWorker(event_executor, transform)` copies the result and
runs the transformation there; `Then` and `OnReady` remain inline.
The lower-level `ThenOn(future, worker, transform)` selects a worker explicitly.

For example, count lines in a downloaded text result on a worker. Here
`downloaded` is a Future produced by your download operation, and `worker` is
an existing executor:

```cpp
#include <algorithm>
#include <cstddef>
#include <string>

#include <absl/base/nullability.h>

#include "symbian/concurrency/worker_executor.h"

symbian::concurrency::Future<std::size_t> CountDownloadedLines(
    const symbian::concurrency::Future<std::string>& downloaded,
    symbian::concurrency::WorkerExecutor* absl_nonnull worker) {
  return symbian::concurrency::ThenOn(
      downloaded, worker,
      [](const absl::StatusOr<std::string>& text)
          -> absl::StatusOr<std::size_t> {
        if (!text.ok()) {
          return text.status();
        }
        if (text->empty()) {
          return std::size_t{0};
        }
        return std::count(text->begin(), text->end(), '\n') +
               (text->back() != '\n');
      });
}
```

The download should already have a body-size limit. The worker must remain
available through completion. This result's inline observers run on the worker;
post any label update to the UI owner's mailbox.

`PostFiber(work, stack_bytes)` schedules a fiber on the worker's own scheduler.
The default stack is 16 KiB; accepted sizes are word-aligned from 4 KiB to
1 MiB. TLS handshakes can need 256 KiB. The admission cap includes queued and
active work; a full or closed executor returns a status.

`Close()` does not block the event thread. `Finish()` reports asynchronous
drainage. A job or fiber that waits forever can prevent drainage. The guest
has no preemption, shared `Post`/`PostAt` pool or complete A11 cancellation tree.

## Fibers, synchronization and channels

Link `Symbian::Fibers` for guest `thread::Fiber` and `thread::Scheduler`.
Fibers and schedulers remain pinned to their creating OS thread. Keep both
alive until the fiber finishes; destroying unfinished ownership is a runtime
contract failure. Fibers do not virtualize OS TLS, heaps or Symbian leave state.

`thread::Mutex`, `MutexLock`, `CondVar` and `SleepFor` park fibers cooperatively.
Outside a fiber they use blocking OS-thread behavior, so keep those calls off
the event thread. `CondVar` follows A11's convention: `true` means timeout and
`false` means signal. `SchedulerPolicy` selects ready ordering and provides a
wake hook; `EventExecutor` supplies the native-wait integration.

`thread::Channel<T>` supports bounded buffering, rendezvous, selection,
cancellation-aware writes and one-time close. `Case`, `PermanentEvent`,
`Select` and `SelectUntil` share one selection protocol. Guest selection
rotates the first case; a finite wall deadline is converted once to monotonic
waiting. A closed-channel write is fatal in the no-exceptions guest profile.
Use `symbian::concurrency::BoundedChannel<T>` for nonblocking, fallible mailbox
operations and idempotent close.

## Host backend and Python

The host archive bundles Boost.Fiber/Context behind a Boost-free public ABI;
consumers need no separate Boost libraries or headers. The host supplies
thread-affine fibers with cooperative cancellation, explicit join, C++ cleanup
and no detach or forced unwind. Shared-pool `Post` and `PostAt` run stackless
callbacks. Host unresolved `Await` parks and honors an absolute deadline.
The adaptation does not supply A11's full work-stealing pool, pooled fiber
`Submit`/`Schedule`, introspection or fiber tree.

Exceptions remain disabled by default. The Boost primitive and pool-teardown
translation units explicitly enable them for their implementation requirements.
Python bindings use A11's CPython park guard and deferred-reference holders:
native waits release the GIL, Python access acquires it, and asyncio completion
returns to the loop thread. Python policy stays outside native libraries.
