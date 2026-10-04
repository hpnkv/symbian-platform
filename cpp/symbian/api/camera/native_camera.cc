// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "native_camera.h"

#include <ECam.h>

namespace symbian::api::camera {

extern "C" int SymbianDeviceCameraCount() {
  return CCamera::CamerasAvailable();
}

}  // namespace symbian::api::camera
