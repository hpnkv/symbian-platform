// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_API_DISPLAY_SCREEN_CONTROL_H_
#define SYMBIAN_API_DISPLAY_SCREEN_CONTROL_H_

#include <cstdint>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace symbian::api::display {

/** @brief A point-in-time copy of the primary display in little-endian RGB565. */
struct ScreenCapture {
  int width = 0;
  int height = 0;
  int stride_bytes = 0;
  std::vector<std::uint8_t> pixels;
};

// These process-independent Window Server operations use a fresh session on
// each call. A capture owns its pixels and can outlive the native session.
absl::StatusOr<ScreenCapture> CapturePrimaryScreen();

enum class PointerAction { kMove, kDown, kUp };

/** @brief Inject one primary-display pointer event at physical pixel coordinates. */
absl::Status SendPrimaryPointerEvent(PointerAction action, int x, int y);

}  // namespace symbian::api::display

#endif  // SYMBIAN_API_DISPLAY_SCREEN_CONTROL_H_
