# Display component

**Implemented:** `Symbian::Display` exports
`ReadPrimaryDisplayGeometry()` in `<symbian/api/display/display.h>` and
Window Server surface helpers. The development agent's fixed control panel
lives in `agent_service/` because its BACK/STOP actions and labels belong to
that application.

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

## Window text overlay

`WindowSurface::DrawTextLines` accepts bounded UTF-16 labels after `Present()`.
It copies the labels and replays them during Window Server redraws, so callers
do not need native redraw events or font handles. The next `Present()` replaces
the overlay; call `DrawTextLines` again for the next frame.
`WrapTextLines` fits UTF-16 report text using the same device font's measured
glyph widths, preserving explicit line breaks.
`UpdateRgb565Frame` maps the SDK-owned Window Server bitmap only for the
duration of a writer callback. The caller can transform into that bitmap and
then call `Present()` without an intermediate application frame buffer.
`SetAutomaticOrientation` requests the device's automatic orientation policy
and updates the application screen mode on screen changes.

## GLES2 texture presentation

`Symbian::GlesDisplay` provides `GlesWindowContext`, `Gles2Texture`, and
`Gles2TexturePresenter` for an exclusive GLES2 context. A texture owns its
allocation and reports an opaque handle for interoperation with frame
backends. The presenter clears the current framebuffer and draws any RGBA8888
texture into a viewport; the caller chooses when to swap. Optional sampling
flags handle blue-first bytes and top-down rows in the fragment shader. These utilities
manage GL objects and presentation without exposing Window Server or EGL
handles to the application. Destroy them while their context is current.

## Restrictions

The geometry query is a snapshot. It does not subscribe to orientation or
size-change notifications and does not enumerate multiple screens.

For an application window, use the [Window Server counter](../../guides/from-source.md)
or the [Symbian Qt button](../../guides/qt-app.md). The Qt example requires guest
Qt 4.8.1 and uses raster graphics and Plastique in the emulator; the host
emulator's desktop Qt libraries do not supply guest widgets.
