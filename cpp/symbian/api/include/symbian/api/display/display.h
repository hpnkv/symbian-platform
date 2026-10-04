// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_API_DISPLAY_DISPLAY_H_
#define SYMBIAN_API_DISPLAY_DISPLAY_H_

#include <optional>

#include "absl/status/statusor.h"

namespace symbian::api::display {

// Primary display geometry reported by HAL. Window Server layout and rotation
// can differ from these hardware dimensions.
struct DisplayGeometry {
  // Current horizontal size in pixels.
  int width_pixels = 0;
  // Current vertical size in pixels.
  int height_pixels = 0;
  // Physical horizontal size in twips, if published by this profile.
  std::optional<int> width_twips;
  // Physical vertical size in twips, if published by this profile.
  std::optional<int> height_twips;
};

// Queries the primary display without owning a Window Server connection.
// This is a snapshot, not an orientation-change subscription. Run it on a
// worker if the event thread must stay bounded-fast.
absl::StatusOr<DisplayGeometry> ReadPrimaryDisplayGeometry();

}  // namespace symbian::api::display

#endif  // SYMBIAN_API_DISPLAY_DISPLAY_H_
