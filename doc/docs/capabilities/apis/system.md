# System component

**Implemented:** `Symbian::System` exports typed readings of the system tick
and fast counters in `<symbian/api/system/counters.h>`, plus an active
service loop in `<symbian/api/system/active_service.h>`.

## Motivation and modernization

The original calls expose a bare 32-bit count and separate integer
period/frequency queries. A count without its unit is easy to misuse as wall
time or to compare across devices. `TickReading` pairs the count with a
`absl::Duration` period; `FastCounterReading` pairs it with the
measured ticks-per-second value. `absl::StatusOr` distinguishes an unavailable
counter from a valid zero count. Both structs make 32-bit wrap explicit.

## Ownership and cost

`counters.cc` uses the SDK's native runtime bridge. The
public header contains no Symbian descriptors or HAL types. Each query samples
one metadata value and one count without heap allocation or a retained native
handle. The metadata call is repeated because the public result must describe
the current platform rather than a cached assumption. These calls are small,
but a caller with a strict event-thread budget should measure its device.

## Measure a short interval

Link `Symbian::System`. Keep a `TickReading` when work starts, then use unsigned
subtraction to include one counter wrap. The interval must be shorter than a
full wrap, and the reported period must stay unchanged.

```cpp
#include "symbian/api/system/counters.h"

absl::StatusOr<absl::Duration> ElapsedSince(
    symbian::api::system::TickReading started) {
  auto now = symbian::api::system::ReadTickCounter();
  if (!now.ok()) {
    return now.status();
  }
  if (now->period != started.period) {
    return absl::FailedPreconditionError("Tick period changed");
  }
  const std::uint32_t ticks = now->count - started.count;
  return now->period * ticks;
}
```

Use the SDK's absolute deadlines for timeouts; this counter calculation is for
short elapsed-time measurements.

## Resident service loop

`RunActiveService` installs the native active scheduler on the calling thread,
arms a process-local stop property, and runs asynchronous service callbacks.
The `start` callback should bind listeners before the stop property is defined;
this lets a second application launch detect an already running instance without
changing its signal. `on_ready` runs once the stop subscription is armed, so a
service can safely start its user interface. A second guest thread calls
`RequestActiveServiceStop` to shut down; a callback already on the scheduler
thread calls `StopActiveService`. Setup and cross-thread signal failures return
`absl::Status`; only the process entry point translates that status to a
native exit reason.

The property category and key are application-owned. Pick a stable pair and
keep it unique. The SDK owns `CActiveScheduler` and `RProperty` lifetime, while
the application owns its listener and worker lifetime. This helper uses the
native active scheduler required by `ActiveTcpListener`; it does not install a
second SDK task scheduler. See the
[resident agent](../../reference/agent-protocol.md) for a complete use.

## Restrictions

Counters wrap at 32 bits; use their reported period or frequency and account
for wrap when computing intervals. This API supplies no wall clock or timer
subscription.
