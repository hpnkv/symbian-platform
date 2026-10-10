# Bounce-style Arkanoid

This example shares one game across SDL2 (`sdl2_app`) and SDL3 (`sdl3_app`).
The default build requests OpenGL ES 2 and falls back to SDL software
rendering if context creation fails. Firmware without `libegl.dll` needs a
software-only build with `-DSYMBIAN_ARKANOID_GPU=OFF`, because the GPU image
imports EGL and GLES2 at load time. The blocks, ball, platform and text are
drawn from compact source assets; no asset files need to be copied to a device.

The source follows the game's responsibilities:

- `main.cc`: entry point.
- `application.h` / `application.cc`: SDL session, window, feedback and frame
  pacing.
- `game.h` / `game.cc`: input, three-level simulation and pause-menu state.
- `renderer.h` / `renderer.cc`: scene composition and pause-menu drawing.
- `assets/primitives.h` / `.cc`: SDL2/SDL3 drawing primitives.
- `assets/block.h` / `.cc`, `ball.h` / `.cc`, `platform.h` / `.cc`: programmatic
  sprites.
- `assets/glyphs.h` / `.cc`: indexed 5×7 bitmaps for all printable ASCII
  characters and a distinct default glyph outside that range.
- `assets/text.h` / `.cc`: bitmap text drawing.
- `assets/icon.svg`: Bounce-inspired application icon compiled into the SIS
  as a Symbian MIF resource.
- `arkanoid_adapter.h`: small SDL2/SDL3 signature differences.

The glyph bitmaps are derived from the public-domain `5x7.bdf` included with
the checked-out Qt 4 research source. `glyphs.cc` records its copyright field.

Move the platform with EKA2L1 host Left/Right arrows or by dragging on a touch
screen. On touch devices, the compact top-left pause button has a generous
invisible touch target. On non-touch devices it is hidden; the right softkey
opens the pause menu. Tap or press Return to launch the ball. Escape/Back also
opens the pause menu; arrows and Return, or touch, choose Resume, Sound On/Off
or Exit. Losing window
focus also pauses play.
EKA2L1 currently reports a digitiser on the non-touch E71 profile, so its
pause button remains visible there; the right softkey still works.

The simulation uses fixed 16 ms steps. Presentation uses monotonic deadlines
capped to the display's reported refresh rate, or 60 Hz if the driver reports
none. The small top-right counter shows measured FPS and `GPU` only when SDL
selected an accelerated renderer. Pointer events move the paddle immediately,
including drag events while the finger remains down. The game chooses MIDI
notes and schedules them on its frame clock; the SDK owns the MIDI service
thread and a bounded note queue. The ball bounces at its crossing of the paddle's top and
is pushed back inside the side and top walls if it penetrates them. A brick or
paddle hit requests a short HWRM pulse through the SDK worker because the
native server transaction is synchronous. The
worker is started before play; the game rate-limits hits and the SDK's
two-second service deadline stops further requests if
it stalls, and the pause menu identifies the stalled stage. EKA2L1 cannot
validate the physical motor or server latency. Its menu feedback can remain
enabled even when HWRM's normal vibration profile is off.

The six rows of brick variations share one small immutable texture atlas, with
the original rectangle drawing as a fallback. SDL2 and SDL3 request display
sync on their GLES2 renderers; the SDK frame pacer avoids an extra frame wait
after a missed deadline. The FPS counter remains the phone-side measurement.

The ARMv6 release preset compiles the simulation and renderer in ARM state.
Measured target assembly showed constant integer division could use a multiply
and shift there, while variable division uses the SDK's ARM compiler-rt helpers.
The baseline does not assume ARM hardware divide or a VFP unit.

From the source workspace, use `uv run symbian app run --project
examples/sdl2_app` for SDL2. Both targets can be built with:

```sh
cmake --preset guest-probes-armv6-release-gpu \
  -DSYMBIAN_NATIVE_CONVERTER=/path/to/symbian-native
cmake --build build/guest-probes-armv6-release-gpu \
  --target sdl2_app_e32 sdl3_app_e32
```

`Symbian::PortableSdl2Gpu` and `Symbian::PortableSdl3Gpu` expose the GPU
archives in an installed SDK. Their software counterparts remain available.
The shared SDK `WindowSurface` supplies pointer, focus and refresh information;
`GlesWindowContext` owns EGL state, and `FramePacer` supplies presentation
deadlines. The SDL C APIs remain usable alongside the SDK's C++ owner wrappers.
