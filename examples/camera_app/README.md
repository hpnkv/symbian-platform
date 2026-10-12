# Camera capture app

The SDK emulator supports real host cameras through Qt Multimedia. Camera 0
uses the host's default video input; macOS camera permission must be granted
to EKA2L1. See the [host-camera investigation and replay](../../doc/docs/reference/emulator-host-camera.md)
for build dependencies, measured results and the opt-in integration test.

Both examples query ECam and reserve camera 0 when one is reported. Two small
rounded buttons overlay the viewfinder: **REAR** (the default) and **ROTATE**.
The selected button has a cyan border. The buttons select presentation policy;
the camera remains camera 0. Left/Right also select Rear/Rotate. The pair stays
centered around the same physical-screen anchor in both modes. The pills and
labels rotate there, with spacing adjusted to keep the same small gap in either
orientation. Touch targets follow the rotated pills.

**Rear camera** always requests a landscape stream, starting with 640×360 and
falling back to 4:3 and smaller sizes. Capture storage, stride and texture
dimensions stay fixed through orientation changes. ECam request dimensions do
not establish a sensor mounting angle: the Nokia 808 bitmap is aligned with the
native display. The renderer compensates only for Window Server rotation, so
the image stays in the same position on the physical glass. Its orientation is
carried separately in `ViewfinderGeometry.orientation` and `oriented_size`.
A consumer saving or transferring a borrowed frame keeps that metadata alongside
the original pixels; swapping raw width/height would corrupt its stride.

**Rotate view** requests 360×640 when selected in portrait or 640×360 in
landscape. These are preferences: the physical phone returned 360×270 for the
portrait request. Both modes preserve the complete returned scene and use
uniform aspect fit, with centered black margins when the frame and screen have
different shapes. A portrait request no longer crops that fallback to a narrow
151×270 strip and magnifies it. Matching frame/screen shapes fill the display.

Switching modes can reopen capture. Changing orientation alone does not. Camera
GL retains the source texture and changes only sampling/presentation metadata
while refreshing the EGL window surface. The software example samples the same
source directly into a fitted screen rectangle and clears its margins. Neither
path allocates a rotated source or assumes that requesting landscape pixels
means adding a 90° rotation. On phones whose bitmap fallback cannot supply the
requested shape, the two modes can show the same framing at different capture
resolutions.

`camera_app` displays borrowed BGR565 or RGBX8888 frames through the Window
Server at up to 30 Hz. `camera_app_gles2` uploads viewfinder frames into a GPU
texture and keeps presentation on the GPU. Its startup screen is a dark camera
glyph with three animated dots, shown before discovery/reservation and while
waiting for the first frame, including after a focus-related camera restart.
It uses the SDK rectangle batch on the EGL surface, so no bitmap overlay can
obscure the live image. The former blue/magenta diagnostic clear is removed.

If no camera is reported, the examples retain their synthetic rendering
diagnostic. Opening, polling and startup timeout errors go to the SDK failure
handler. Tap outside the buttons or press Select/Enter to change the resampling
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
