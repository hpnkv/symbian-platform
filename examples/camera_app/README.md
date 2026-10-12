# Camera capture app

The SDK emulator supports real host cameras through Qt Multimedia. Camera 0
uses the host's default video input; macOS camera permission must be granted
to EKA2L1. See the [host-camera investigation and replay](../../doc/docs/reference/emulator-host-camera.md)
for build dependencies, measured results and the opt-in integration test.

Both examples query ECam and reserve camera 0 when one is reported. The
viewfinder uses one fixed placement on the physical screen, keeping the camera's
native pixel axes. A landscape-shaped bitmap does not establish how the sensor
is mounted: turning it merely to fit the panel made the phone scene sideways
in 1.0.30.

Placement minimizes margins without enlarging source pixels. It uses the largest
rectangle allowed by both the native screen and the returned frame dimensions,
then takes a proportional centered source crop for that rectangle. At least one
complete source axis remains visible. Overflow on the other axis is cropped;
if a source axis is shorter than the screen, the remaining margin is centered.
The source buffer itself stays unchanged.

The phone trace now reports a 640×480 source. On its 360×640 native drawable,
source rectangle (140,0,360,480) maps at unit scale to the centered 360×480
viewport, with 80-pixel top/bottom margins. This preserves the previous image
footprint while correcting the sideways scene; fitting the whole upright frame
would instead leave 185-pixel margins. Smaller 480×360 frames use a centered
360×360 crop at unit scale. Frames that cover both screen axes can scale down
to fill the screen. Capture preferences begin with 640×480. Request dimensions
alone do not establish optical field of view.

After that initial choice, turning the phone changes only orientation metadata
and the overlay. The GL app keeps its native EGL drawable, source texture,
sampling bounds and native sampling angle. It does not refresh the
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
The complete glyph and dot animation follow the current UI orientation.
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
