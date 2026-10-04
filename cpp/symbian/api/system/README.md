# System component

**Implemented:** `Symbian::System` exports typed readings of the system tick
and fast counters in `<symbian/api/system/counters.h>`.

## Motivation and modernization

The original calls expose a bare 32-bit count and separate integer
period/frequency queries. A count without its unit is easy to misuse as wall
time or to compare across devices. `TickReading` pairs the count with a
`std::chrono::microseconds` period; `FastCounterReading` pairs it with the
measured ticks-per-second value. `absl::StatusOr` distinguishes an unavailable
counter from a valid zero count. Both structs make 32-bit wrap explicit.

## Boundary and cost

`counters.cc` uses the SDK's already verified native runtime bridge. The
public header contains no Symbian descriptors or HAL types. Each query samples
one metadata value and one count without heap allocation or a retained native
handle. The metadata call is repeated because the public result must describe
the current platform rather than a cached assumption. These calls are small,
but a caller with a strict event-thread budget should measure its device.

## Remaining work

Counter wrap over long intervals and physical-phone frequencies need
independent checks. This library deliberately provides no wall clock or timer
subscription; those have different semantics and ownership.
