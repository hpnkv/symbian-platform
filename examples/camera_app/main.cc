// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "application.h"
#include "trace.h"

#include <array>
#include <string_view>

#include <absl/base/nullability.h>

#include "absl/status/status.h"
#include "symbian/api/system/failure_handler.h"

namespace {

absl::Status RunCamera() {
  if (camera_app::Run() == 0) {
    return absl::OkStatus();
  }
  absl::Status error = camera_app::FailureStatus();
  return error.ok() ? absl::InternalError("Camera application returned an error")
                    : error;
}

}  // namespace

int main() {
#if defined(SYMBIAN_CAMERA_GPU_PROBE)
  constexpr std::array<std::u16string_view, 1> logs{
      u"E:\\Others\\camera_app_gles2_trace.txt"};
#else
  constexpr std::array<std::u16string_view, 1> logs{
      u"E:\\Others\\camera_app_trace.txt"};
#endif
  return symbian::api::system::RunWithFailureHandler(
      RunCamera, {.caption = "Camera failure", .log_paths = logs});
}
