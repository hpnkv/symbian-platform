// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include <array>
#include <string_view>

#include <absl/base/nullability.h>

#include "absl/status/status.h"
#include "application.h"
#include "symbian/api/system/failure_handler.h"
#include "trace.h"

int main() {
#if defined(SYMBIAN_CAMERA_GPU_PROBE)
  constexpr std::array<std::u16string_view, 1> logs{
      u"E:\\Others\\camera_app_gles2_trace.txt"};
#else
  constexpr std::array<std::u16string_view, 1> logs{
      u"E:\\Others\\camera_app_trace.txt"};
#endif
  return symbian::api::system::RunWithFailureHandler(
      camera_app::Run, {.caption = "Camera failure", .log_paths = logs});
}
