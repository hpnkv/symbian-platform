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

A positive count does not authorize enabling a capture action; this component
provides discovery only.
