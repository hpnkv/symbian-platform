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

Both modes fill the screen with a proportional, centered crop of the actual
returned bitmap. One complete source axis remains visible; only the excess on
the other axis is hidden. Filling portrait with a landscape frame necessarily
removes side content. There is no extra zoom factor. ECam request dimensions
do not establish the delivered aspect or sensor mounting angle: the Nokia 808
returned 480×360 for a landscape request and 360×270 for a portrait request.

**Rear camera** (default) requests a landscape stream, starting with 640×360.
Its crop uses the native portrait screen aspect and remains fixed on the physical
glass. Only presentation metadata compensates Window Server rotation. For a
480×360 phone frame on the 360×640 screen, the source rectangle is the centered
202×360 strip. Turning the phone preserves this rectangle, the original capture
storage, stride and texture dimensions. Saving/transferring consumers keep
`ViewfinderGeometry.orientation` alongside the borrowed source; swapping raw
width/height would corrupt its stride.

**Rotate view** requests 360×640 when selected in portrait or 640×360 in
landscape. It follows the logical UI without Rear's inverse Window Server
rotation, and recalculates the crop for that UI aspect. With a 480×360 source,
portrait uses 202×360 and landscape uses 480×270. Thus the modes can agree in
portrait, but differ after an orientation change: Rear stays fixed on the glass,
while Rotate turns with the controls and exposes a wider landscape composition.

Switching modes can reopen capture. Changing orientation alone does not. Camera
GL samples the crop from the retained texture using SDK `AspectFillViewport`;
software borrows it with SDK `CropFrameView` and samples directly into the screen
bitmap. Neither path allocates a rotated or recropped source. Both preserve
proportions to integer-pixel rounding. The previous 1.0.28 aspect-fit fallback
preserved every edge but left large margins and made the modes behave alike;
this screen-filling policy supersedes it.

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
