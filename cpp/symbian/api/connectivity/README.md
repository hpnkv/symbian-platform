# Connectivity component

**Planned, not exported:** this directory has no archive or public header yet.

## Motivation and proposed modernization

Historical connection services expose sessions, request statuses and numeric
bearer/state codes. An observation API should represent a connection snapshot
with a typed state, bearer and explicitly unknown fields. It must distinguish
"no active connection" from "the service could not be queried." Opening a
monitor must not create an access point, enable mobile data or change radio
state as a side effect. A move-only observer would own its native session;
`absl::StatusOr` and an SDK `Future` would represent setup and completion.

## Concurrency and cost

Native notifications should be translated on the existing `EventExecutor`
with bounded-fast work, then delivered through an explicit bounded queue.
User parsing and policy callbacks belong on a worker. Reuse one monitor
session for a subscription instead of polling or reconnecting on every event.
Each completed request should expose its data immediately rather than wait
for an entire connection history. A cancellation request must stop new
requests promptly, cancel and drain the in-flight native request, then close
the session. Queue capacity and overflow policy must be explicit so a slow
consumer cannot silently grow memory.

## Evidence before implementation

The prepared checkout does not yet include a connection-monitor client header
or verified import contract. The emulator's current monitor implementation
also returns a fixed connection count and GPRS bearer, so its output cannot
prove actual connectivity. Service ordinals, permissions, notification
ordering, cancellation latency and disconnect behavior need a named-firmware
probe before this component is exported.
Network configuration and radio control require separate authority and are
outside the initial read-only surface.
