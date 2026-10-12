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
falling back to 4:3 and smaller sizes. Once ECam supplies its actual frame size,
the app chooses the same centered crop it would use on a landscape screen. For
example, 640×480 retains all 640 columns and the middle 360 rows. That crop,
source storage, stride and texture dimensions stay fixed through orientation
changes. The renderer compensates for Window Server rotation so the image stays
in the same position on the physical glass. Its portrait/landscape interpretation
is carried separately in `ViewfinderGeometry.orientation` and `oriented_size`.
A consumer saving or transferring a borrowed frame keeps that metadata alongside
the original pixels; swapping raw width/height would corrupt its stride.

**Rotate view** keeps the previous behavior: it requests 360×640 when selected
in portrait or 640×360 in landscape, then rotates presentation relative to that
opening orientation. Each mode retains one complete source axis and crops only
the centered excess on the other, with no additional zoom or stretching. Both
use the actual dimensions returned by ECam, including fallback sizes.

Switching modes can reopen capture and choose a new crop. Changing orientation
alone does neither. Camera GL retains the same source texture and changes only
sampling/presentation metadata while refreshing the EGL window surface. The
software example borrows the same crop and samples into the current screen
bitmap. A rear sensor is assumed to be mounted in the device's normal landscape
orientation (90° from its native portrait display); physical-phone validation
of that assumption remains necessary.

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
