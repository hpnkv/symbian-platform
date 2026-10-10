# Camera capture app

`camera_app` queries ECam, reserves camera 0 when one is reported, and displays
borrowed BGR565 or RGBX8888 viewfinder frames through the Window Server at up
to 30 Hz. It requests a landscape 640x480 frame first, followed by smaller
landscape fallbacks. The capture request stays fixed when the phone rotates.
The Nokia 808 bitmap viewfinder places portrait-aligned content inside that
frame. Both examples now rotate presentation in landscape mode and keep it
unrotated in portrait. A centered aspect crop fills the screen without
stretching the image or allocating a rotated source buffer. Solid symmetric
side margins in bitmap frames are excluded before choosing the crop; other
frames retain their full bounds. Camera GL keeps the last texture for an
immediate redraw when the display rotates.
If opening or polling fails, it switches to a moving synthetic RGBA diagnostic.
Tap or press
Select/Enter to switch between nearest and bilinear resampling; press Back
or Escape to exit. The top strip shows discovery (green: a camera is reported,
yellow: zero, red: query failed), filter (blue: nearest, cyan: bilinear), and
stream state (green: captured frame, blue: waiting, gray: synthetic).

`camera_app_gles2` uploads borrowed RGBX8888, BGR565 or Gray8 viewfinder data
into a GPU texture,
scales it into the presentation texture and displays it from GPU memory. It
uses synthetic BGR565 pixels through the same upload path if capture cannot
start. Tap or press
Select/Enter to compare nearest and bilinear filtering. Its top strip is green
for the live stream, yellow for the synthetic diagnostic, and red if discovery
failed. This target requires EGL and GLES2; configure with
`-DSYMBIAN_CAMERA_GPU_PROBE=OFF` on devices without those libraries.

The ECam adapter borrows a mapped `RChunk` or descriptor frame. Frame leases
release native buffers on scope exit; stream destruction stops the viewfinder
and defers final camera cleanup until any outstanding lease is released.
Both operations must run on the thread that opened the camera. The top discovery
strip reports only a count; the stream strip distinguishes live frames from
the synthetic diagnostic.
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
