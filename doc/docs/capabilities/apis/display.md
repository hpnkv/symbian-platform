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

## Ownership and cost

`native_display.cc` contains the legacy HAL include; public code sees ordinary
C++ values. Four small synchronous HAL reads need no Window Server connection,
heap buffer or long-lived session. Results are not cached because a display
profile can change. A caller that cannot tolerate those calls on an event
thread can run the query on a worker.

## Choose an initial grid density

Link `Symbian::Display`. Use the primary width to choose an initial thumbnail
column count; confirm the actual client rectangle with Window Server when the
window opens or its size changes.

```cpp
#include <algorithm>

#include "symbian/api/display/display.h"

absl::StatusOr<int> InitialThumbnailColumns() {
  auto screen = symbian::api::display::ReadPrimaryDisplayGeometry();
  if (!screen.ok()) {
    return screen.status();
  }
  return std::clamp(screen->width_pixels / 160, 1, 3);
}
```

This uses pixel geometry rather than inventing a DPI from missing twip fields.

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
An optional `heading_provider` supplies a static label for changing service
state. The panel calls it on the Window Server thread and checks for a new
label on its existing wake timer, redrawing only after a change. Return
immutable strings that remain valid until the panel closes; share the state
with a worker through an atomic value.

## Restrictions

The geometry query is a snapshot. It does not subscribe to orientation or
size-change notifications and does not enumerate multiple screens.

For an application window, use the [Window Server counter](../../guides/from-source.md)
or the [Symbian Qt button](../../guides/qt-app.md). The Qt example requires guest
Qt 4.8.1 and uses raster graphics and Plastique in the emulator; the host
emulator's desktop Qt libraries do not supply guest widgets.
