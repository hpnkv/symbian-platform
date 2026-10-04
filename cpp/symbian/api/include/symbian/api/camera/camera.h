// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_API_CAMERA_CAMERA_H_
#define SYMBIAN_API_CAMERA_CAMERA_H_

#include <cstdint>

#include "absl/status/statusor.h"

namespace symbian::api::camera {

// A snapshot of camera discovery. An index is meaningful only while the
// device configuration remains unchanged; it is not a persistent identity.
struct CameraInventory {
  // Number of camera slots reported by ECam. Zero means none are available.
  std::uint32_t available_count = 0;
};

// Queries ECam without reserving a camera or beginning capture. No camera
// permission or operating mode is inferred from the count alone.
absl::StatusOr<CameraInventory> DiscoverCameras();

}  // namespace symbian::api::camera

#endif  // SYMBIAN_API_CAMERA_CAMERA_H_
