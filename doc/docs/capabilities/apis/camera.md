# Camera component

**Implemented first slice:** `Symbian::Camera` exports `DiscoverCameras()` in
`<symbian/api/camera/camera.h>`. It queries ECam's available camera count
through a small native bridge. The typed result does not claim identity,
orientation, permission or capture modes.

## Motivation and modernization

The original `CCamera` API is callback-based and uses leaves, explicit camera
reservation, power state and frame buffers whose lifetime depends on the
callback. Ordinary application code should not need those classes to ask if a
camera exists. `CameraInventory` has a documented count; native negative
results become `absl::StatusOr` errors rather than being mistaken for zero
cameras. The ECam header and frozen import remain isolated in
`native_camera.cc`; the SDK also packages the header for explicit legacy use.

Discovery does not reserve or power on hardware or start a preview. It is
synchronous and stateless, so it requires no event loop or secondary scheduler.

## Inspection and capture boundary

The preserved ARM EABI `euseru.def` lacks exports for `TTrap::Trap` and
`TTrap::UnTrap`: this ABI uses the exception-based leave mode. A candidate
inspection translation unit with that mode linked after adding the frozen
`drtaeabi` exception imports. However, its imported C++ type-info vtable is
a data import, and the current ELF converter accepts only function imports in
this path. The candidate therefore failed ELF-to-E32 conversion and was
removed from the public library. Opening a camera without trapping a possible
leave would be unsafe. The SDK needs a verified data-import/leave boundary
before exposing inspection, reserve or capture.

A future camera lease should own `CCamera` and its observer together, with
reserve, power-on and capture represented as explicit states. Still capture
should return an SDK `Future` carrying owned image bytes and metadata;
cancellation must cancel and drain the native request before releasing the
lease. Preview requires a bounded reusable frame pool and a documented drop
policy. The event thread should only hand off frame ownership and small
metadata; image conversion and encoding belong on workers.

## Evidence and limits

The source contract is the preserved Symbian multimedia `ECam.h` and
`ecamU.def`. Its snapshot is pre-Belle. Host tests cover discovery and native
error mapping; the packaged ARMv5T/Dyncom guest probe exercised discovery.
Neither proves a working camera pipeline, image quality or Nokia 808 behavior.
