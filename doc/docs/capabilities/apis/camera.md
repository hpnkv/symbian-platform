# Camera discovery

Link `Symbian::Camera` and include `<symbian/api/camera/camera.h>` to call
`DiscoverCameras()`. It queries ECam's available camera count without reserving
or powering hardware or starting preview.

`CameraInventory` carries the count. Native negative results become
`absl::StatusOr` errors rather than zero cameras. The query is synchronous and
stateless and needs no event loop. Original ECam classes and the frozen import
are isolated in the native bridge; the SDK also includes the legacy header.

The API does not report camera identity, orientation, permission or capture
modes. Camera opening, reservation, preview and capture are unsupported. Those
legacy operations can leave and require request and buffer lifetimes that this
discovery API does not supply.
