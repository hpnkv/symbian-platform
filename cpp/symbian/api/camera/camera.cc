// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "symbian/api/camera/camera.h"

#include "native_camera.h"
#include "symbian/native_status.h"

namespace symbian::api::camera {

absl::StatusOr<CameraInventory> DiscoverCameras() {
  const int count = SymbianDeviceCameraCount();
  if (count < 0) {
    return symbian::StatusFromNativeError(count, "Discover cameras");
  }
  return CameraInventory{static_cast<std::uint32_t>(count)};
}

}  // namespace symbian::api::camera
