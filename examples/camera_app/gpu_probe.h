// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_CAMERA_APP_GPU_PROBE_H_
#define SYMBIAN_CAMERA_APP_GPU_PROBE_H_

#include <absl/base/nullability.h>

namespace symbian::api::display {
class WindowSurface;
}

namespace camera_app {

int RunGpuWindow(symbian::api::display::WindowSurface* absl_nonnull window);

}  // namespace camera_app

#endif  // SYMBIAN_CAMERA_APP_GPU_PROBE_H_
