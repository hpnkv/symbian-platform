# Camera streams and frames

Link `Symbian::Camera` and include `<symbian/api/camera/camera.h>` to call
`DiscoverCameras()`. It queries ECam's available camera count without reserving
or powering hardware or starting preview.

`CameraInventory` carries the count. Native negative results become
`absl::StatusOr` errors rather than zero cameras. The query is synchronous and
stateless and needs no event loop. Original ECam classes and the frozen import
are isolated in the native bridge; the SDK also includes the legacy header.

`<symbian/api/camera/frame.h>` describes a frame's format, dimensions, plane
strides, capture timestamp, sequence and memory kind. Linear and mapped RAM
use spans; GLES2 textures and other native images carry opaque handles and a
device token. `TransformFrame` copies equal-size frames row by row or resamples
with nearest/bilinear filtering. RGB565, RGBA8888, Gray8 and planar YUV420
have stride-aware RAM paths with no intermediate allocation. When an image is
linear or mapped, Gray8, RGB565 and planar YUV420 can also convert directly
to RGBA8888 while resizing, without a second full-frame pass. RGBA8888 and
Gray8 can likewise convert directly to RGB565 for a software display frame;
RGBA alpha is discarded by that conversion.
`kGray16`, `kRgb161616` and `kYuv420Planar16` carry 10–16 significant bits
per channel in little-endian 16-bit samples. Their RAM copy and resampling
paths keep full sample precision; Gray16 and RGB161616 can be converted to
8-bit RGBA or RGB565 for preview. The preview conversion scales the recorded
sample range to 8 bits and does not apply a transfer function or tone map.
`kBayer16` carries 10–16-bit raw photosites with an explicit CFA pattern;
the common RAM utility permits an equal-layout copy but refuses to resample
the mosaic as if it were color pixels. Backends can handle native images and
other transfers directly through `FrameBackend`. High-depth YUV color preview
and raw demosaicing need an explicit device/color pipeline.
YUV conversion
requires an explicit BT.601/BT.709 range; freshness checks require an explicit
monotonic clock domain, so unknown device metadata is not guessed. When an image is
opaque, the operation dispatches to a `FrameBackend` and never silently maps
or downloads it. A backend can reject an unsupported pairing. `FrameLease`
also lets a device backend tie buffer release to scope.

`<symbian/api/camera/camera_stream.h>` supplies an ECam viewfinder stream.
`CameraSource` is the shared capture interface for device backends: callers
request ordered layouts, poll leased frames or consume a scoped frame, and close
the stream. The ECam
`CameraStream` implements it; another backend can return high-depth mapped,
texture or native-image frames without changing capture consumers.
`PollScoped(FrameConsumer*)` invokes the consumer before callback-owned memory
can be reused. Consumers run synchronously on the opening thread and cannot
retain the view or close the source. The ECam bitmap backend maps `EColor16MU`
and `EColor64K` in this path without a CPU frame copy. `Poll()` retains its
movable lease contract and copies ECam V1 bitmaps into owned slots because that
observer does not transfer bitmap ownership.
`Open(index, preferences)` tries a caller ordered list of Gray8, BGR565,
RGBX8888 or planar YUV420 viewfinder formats after reserving and powering the
camera asynchronously. `Poll()` pumps ready active objects
without blocking. A busy reservation is retried at 500 ms intervals up to six
times; if the camera remains unavailable, polling returns a camera-in-use
status with the native error code. A returned `FrameLease` borrows either a
mapped `RChunk` or
a native descriptor; destroying it releases the buffer. `Close()` and the
stream destructor stop capture and release camera ownership. Capture, polling,
and lease release use the opening OS thread. ECam may adjust the requested
size, which the returned layout reports. Unsupported formats and malformed
buffer layouts return status errors. ECam's packed 565 puts red in the low
bits, unlike display RGB565; the distinct `kBgr565` format preserves that
meaning. ECam's 32-bit RGB leaves its fourth byte unused, represented as
`kRgbx8888`. Planar YUV uses the ECam documented BT.601 encoding and currently
requires tightly packed planes. The adapter reports a monotonic timestamp
at callback delivery, which is later than sensor exposure time. The process
needs the Symbian `UserEnvironment` capability to create an ECam camera.
If all formatted viewfinder requests return `KErrNotSupported` and the camera
advertises bitmap viewfinder support, the SDK reopens it through the bitmap
observer. The lease path copies bitmap callbacks into two SDK-owned frame
slots, exposing `EColor64K` as RGB565 and `EColor16MU` in its native BGRX byte
order. A
leased slot is never overwritten; closing the stream releases its camera
even while a copied frame lease remains. The Nokia 808 has physically delivered
both 320x240 and 640x480 `EColor16MU` bitmap viewfinder frames.
Both example paths request a landscape 640x480 viewfinder first, then smaller
landscape fallbacks. ECam chooses the final frame size. The capture request
does not change when the phone rotates.

`Symbian::CameraGles2` supplies `Gles2FrameBackend` for RGBA8888 texture
copy/resampling and direct BGR565, RGBX8888, Gray8 or RGB565 RAM upload in the
current GL context. For a tightly packed mapped BGRX frame it can upload raw
bytes into a texture without staging or CPU channel conversion; the presenter
handles channel order and top-down rows in its shader.
The GLES2 path converts ECam's red-low BGR565 rows to RGBA during upload
because GLES2's native 565 upload interprets red in the high bits. RGBA
texture resampling then stays on the GPU. High-depth textures require a
backend with suitable device formats; the GLES2 backend reports unsupported
for them instead of silently reducing precision.
Texture-to-texture work stays on
the GPU. Explicit RAM/texture transfers upload or read back top-down rows.
Client upload pixels may be released when `glTexSubImage2D` returns; subsequent
work in the same context stays ordered without a full GPU drain. Readback
waits for the requested pixels and can still stall the caller. Readback uses one
GPU transfer and a temporary RAM buffer to flip rows into the destination
stride. A dedicated GL context can use `Gles2ContextUse::kExclusive` to skip
state queries and restoration. The caller owns textures, the current context
and any cross-context input fence. Other camera or
display engines can implement `FrameBackend` without changing the frame API.
`FrameIsFresh` compares capture time with a presentation deadline; the
existing `FramePacer` owns display timing, so the camera API adds no scheduler.
`CenteredAspectCrop` selects a centered source rectangle that preserves the
full source height for a tall display and the full source width for a wide
display. `CropFrameView` borrows that rectangle from linear or mapped memory
without copying; planar YUV420 crops require even boundaries and dimensions.
`TransformFrame` accepts a `FrameRotation` for direct linear or mapped RGB565
output. Its resampler combines rotation, scaling and format conversion in one
destination write; opaque GLES2 presentation rotates in the shader instead.
On the Nokia 808, ECam's bitmap viewfinder returns portrait-aligned content
inside the requested landscape-sized frame. The examples rotate presentation
for landscape display mode and leave it unrotated for portrait. Solid
symmetric black side margins can be identified by `DetectSolidSideMargins` in
packed CPU-accessible frames; uncertain or opaque frames keep their full
bounds. The software path borrows a centered crop of that content and writes
directly into the Window Server bitmap with
the rotation sampled into that destination. Camera GL uses an aspect-fill
viewport and shader rotation with a source region, and redraws its retained
texture immediately after display rotation. It allocates no rotated texture
or CPU buffer.

The `examples/camera_app` app opens an ECam viewfinder when available and
otherwise displays a labeled synthetic diagnostic. Both live paths use
`PollScoped`: Camera GL uploads the mapped bitmap to one texture, while Camera
converts directly into a mapped Window Server RGB565 bitmap. Their display
rectangles preserve the capture aspect ratio and fill the display with a
centered crop.

Other device capture backends can wrap a valid descriptor, mapped memory,
texture or native image as a `FrameView` and supply a lease release callback.
The shared transform API dispatches opaque memory to the selected backend.
The bitmap fallback has displayed actual sensor frames on the Nokia 808;
steady throughput and orientation remain under physical investigation.
The ECam `TFormat` values in the available platform header describe 8-bit
monochrome/YUV and 565/888 RGB, with no verified 10-bit viewfinder selection.
The high-depth frame formats are ready for other capture backends; this ECam
adapter does not claim a 10-bit sensor output path.

## Show an inventory label

Link `Symbian::Camera`. Discovery can populate a settings label without opening
or reserving a camera:

```cpp
#include <string>

#include "symbian/api/camera/camera.h"

absl::StatusOr<std::string> CameraLabel() {
  auto cameras = symbian::api::camera::DiscoverCameras();
  if (!cameras.ok()) {
    return cameras.status();
  }
  if (cameras->available_count == 0) {
    return "No cameras reported";
  }
  return std::to_string(cameras->available_count) + " camera slots";
}
```

A positive count does not establish that reservation or capture succeeds.
