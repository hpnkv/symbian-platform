# Display component

**Implemented:** `Symbian::Display` exports
`ReadPrimaryDisplayGeometry()` in `<symbian/api/display/display.h>`.

## Motivation and modernization

The original HAL returns individual integer attributes. The adapter combines
the two required pixel dimensions into one `DisplayGeometry` value and keeps
physical twip dimensions as independent `std::optional<int>` values. This
prevents one failed attribute from silently becoming zero or a guessed DPI.
`absl::StatusOr` rejects nonpositive pixel geometry. The type describes HAL
geometry only: a Window Server layout may differ after rotation.

## Boundary and cost

`native_display.cc` contains the legacy HAL include; public code sees ordinary
C++ values. Four small synchronous HAL reads need no Window Server connection,
heap buffer or long-lived session. Results are not cached because a display
profile can change. A caller that cannot tolerate those calls on an event
thread can run the query on a worker.

## Remaining work

Window Server orientation and size-change notifications, multiple screens and
display-handle ownership require a separate observed contract. This snapshot
does not claim to provide those behaviors.
