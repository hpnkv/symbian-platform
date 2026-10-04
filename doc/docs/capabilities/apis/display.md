# Display component

**Implemented:** `Symbian::Display` exports
`ReadPrimaryDisplayGeometry()` in `<symbian/api/display/display.h>` and a
resident control panel in `<symbian/api/display/resident_panel.h>`.

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

## Resident control panel

`RunResidentPanel` creates a minimal Window Server view for a manually started
background service. The app supplies a UID, caption, short uppercase labels,
and a property identity; the SDK owns the window session, rendering, and
foreground signal. BACK lowers the panel while the service keeps running. STOP
sets the shared atomic stop flag; the service then signals its active loop.
A second app launch may call `RequestResidentPanelForeground` to bring the
existing view back. Run the panel on a separate guest thread so the service
thread can continue accepting connections. This is a small built-in UI, with a
limited bitmap alphabet and fixed layout, rather than a general widget kit.

## Remaining work

Window Server orientation and size-change notifications, multiple screens and
display-handle ownership require a separate observed contract. This snapshot
does not claim to provide those behaviors.
