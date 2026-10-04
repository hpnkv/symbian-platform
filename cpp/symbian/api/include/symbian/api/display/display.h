// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_API_DISPLAY_DISPLAY_H_
#define SYMBIAN_API_DISPLAY_DISPLAY_H_

#include <optional>

#include "absl/status/statusor.h"

namespace symbian::api::display {

/**
 * @brief Primary display geometry reported by the native HAL.
 *
 * Window Server layout and rotation can differ from these dimensions.
 */
struct DisplayGeometry {
  /** @brief Current horizontal size in pixels. */
  int width_pixels = 0;
  /** @brief Current vertical size in pixels. */
  int height_pixels = 0;
  /** @brief Physical horizontal size in twips, when reported. */
  std::optional<int> width_twips;
  /** @brief Physical vertical size in twips, when reported. */
  std::optional<int> height_twips;
};

/**
 * @brief Query primary HAL geometry without a Window Server connection.
 *
 * This is a snapshot, not an orientation-change subscription. Use a worker
 * when the event thread must stay responsive.
 */
absl::StatusOr<DisplayGeometry> ReadPrimaryDisplayGeometry();

}  // namespace symbian::api::display

#endif  // SYMBIAN_API_DISPLAY_DISPLAY_H_
