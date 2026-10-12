# Camera capture app

The SDK emulator supports real host cameras through Qt Multimedia. Camera 0
uses the host's default video input; macOS camera permission must be granted
to EKA2L1. See the [host-camera investigation and replay](../../doc/docs/reference/emulator-host-camera.md)
for build dependencies, measured results and the opt-in integration test.

Both examples query ECam and reserve camera 0 when one is reported. The
viewfinder uses one fixed placement on the physical screen. From the actual
returned frame and native drawable dimensions, it compares the original and
quarter-turned axis alignments and selects the one occupying the most screen
area. The entire frame is visible, proportionally fitted and never enlarged
above one output pixel per source pixel. Equal-area alignments preserve the
original alignment. Remaining margins are centered.

For a 480×360 phone frame on a 360×640 physical screen, the chosen alignment
is 360×480, with 80-pixel top and bottom margins. Every source column and row
remains visible, replacing the narrow 202×360 crop used in 1.0.29. A 640×480
frame fits into the same rectangle at 0.75 scale. Smaller frames are not enlarged.
Capture preferences begin with 640×480, favoring the full 4:3 source over a
16:9 request. Request dimensions alone do not establish optical field of view:
the Nokia 808 previously returned 480×360 for a landscape request.

After that initial choice, turning the phone changes only orientation metadata
and the overlay. The GL app keeps its native EGL drawable, source texture,
full-frame sampling bounds and initial sampling angle. It does not refresh the
surface, restart capture, rotate a source buffer or reconsider its placement
on a display event. A genuine change in delivered source dimensions can select
a new placement. Saving/transferring consumers retain
`ViewfinderGeometry.orientation` alongside the original source; swapping raw
width/height would corrupt its stride.

The Rotate mode has been removed. A small rounded **SMOOTH** control toggles
nearest/bilinear filtering. Its label rotates around the same physical-screen
anchor; it has a cyan border when enabled. Software presentation samples the
original source directly into the logical screen bitmap, compensating Window
Server's display transform without allocating a rotated source.

`camera_app` displays borrowed BGR565 or RGBX8888 frames through the Window
Server at up to 30 Hz. `camera_app_gles2` uploads viewfinder frames into a GPU
texture and keeps presentation on the GPU. Its startup screen is a dark camera
glyph with three animated dots, shown before discovery/reservation and while
waiting for the first frame, including after a focus-related camera restart.
It uses the SDK rectangle batch on the EGL surface, so no bitmap overlay can
obscure the live image. The former blue/magenta diagnostic clear is removed.

If no camera is reported, the examples retain their synthetic rendering
diagnostic. Opening, polling and startup timeout errors go to the SDK failure
handler. Tap the screen or press Select/Enter to change the resampling
filter; press Back
or Escape to exit. The GLES target requires EGL and GLES2; configure with
`-DSYMBIAN_CAMERA_GPU_PROBE=OFF` on devices without those libraries.

The ECam adapter borrows a mapped `RChunk` or descriptor frame. Frame leases
release native buffers on scope exit; stream destruction stops the viewfinder
and defers final camera cleanup until any outstanding lease is released.
Both operations must run on the thread that opened the camera.
GLES2 readback remains an explicit SDK operation, but EKA2L1 stalls during
its `glReadPixels` bridge on the tested RM-807 fixture; this app therefore
keeps its visual path on the GPU. Firmware sensor capture and physical camera
behavior still need separate testing.

With an installed SDK selected in `sdk-location.json`:

```sh
symbian build --project examples/camera_app --output .symbian/camera-app
symbian emu run --project examples/camera_app --firmware my-phone
```

The root source workspace also exposes `camera_app`, `camera_app_gles2`,
and their E32 publishers in its guest presets. The standalone CMake preset
keeps `compile_commands.json` available.
