// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef EXAMPLES_CAMERA_APP_VIEWFINDER_PRESENTATION_H_
#define EXAMPLES_CAMERA_APP_VIEWFINDER_PRESENTATION_H_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>

#include <absl/status/status_macros.h>

#include "capture_preferences.h"

namespace camera_app {

// Source storage stays in producer order. Native placement is chosen from the
// actual frame once; only logical orientation metadata changes with the UI.
struct ViewfinderGeometry {
  symbian::api::camera::FrameRect crop;
  symbian::api::camera::FrameRotation orientation;
  symbian::api::display::WindowSize oriented_size;
  symbian::api::camera::FrameRotation native_orientation;
  symbian::api::display::WindowSize native_size;
  symbian::api::camera::FrameRect native_viewport;
};

// Show all pixels, maximizing occupied area without exceeding unit scale.
inline absl::StatusOr<symbian::api::camera::FrameRect> PreviewFit(
    symbian::api::display::WindowSize image,
    symbian::api::display::WindowSize screen) {
  ABSL_ASSIGN_OR_RETURN(
      auto fit, symbian::api::camera::CenteredAspectFit(
                    image.width, image.height, screen.width, screen.height));
  if (fit.width > image.width || fit.height > image.height) {
    fit = {(screen.width - image.width) / 2, (screen.height - image.height) / 2,
           image.width, image.height};
  }
  return fit;
}

class ViewfinderPresentation final {
 public:
  explicit ViewfinderPresentation(symbian::api::display::WindowSize native_size)
      : native_size_(native_size) {}

  auto preferences() const { return CapturePreferences(); }

  absl::StatusOr<ViewfinderGeometry> Describe(
      int source_width, int source_height,
      symbian::api::display::DisplayRotation display) {
    namespace camera = symbian::api::camera;
    if (source_width <= 0 || source_height <= 0) {
      return absl::InvalidArgumentError("Invalid viewfinder dimensions");
    }
    // Genuine capture renegotiation can change the source dimensions. Display
    // rotation cannot: it is deliberately absent from the placement cache key.
    if (!placement_.has_value() || source_width != source_size_.width ||
        source_height != source_size_.height) {
      ABSL_ASSIGN_OR_RETURN(
          auto straight,
          PreviewFit({source_width, source_height}, native_size_));
      ABSL_ASSIGN_OR_RETURN(
          auto turned, PreviewFit({source_height, source_width}, native_size_));
      const bool quarter_turn =
          static_cast<std::int64_t>(turned.width) * turned.height >
          static_cast<std::int64_t>(straight.width) * straight.height;
      source_size_ = {source_width, source_height};
      placement_ = ViewfinderGeometry{
          .crop = {0, 0, source_width, source_height},
          .orientation = camera::FrameRotation::k0,
          .oriented_size = {},
          .native_orientation = quarter_turn
                                    ? camera::FrameRotation::kClockwise90
                                    : camera::FrameRotation::k0,
          .native_size = {quarter_turn ? source_height : source_width,
                          quarter_turn ? source_width : source_height},
          .native_viewport = quarter_turn ? turned : straight};
    }
    auto geometry = *placement_;
    // Metadata for consumers using logical screen coordinates. The native
    // GLES renderer uses the cached native fields, never this changing angle.
    const int turn = (static_cast<int>(geometry.native_orientation) -
                      static_cast<int>(display) + 4) %
                     4;
    geometry.orientation = static_cast<camera::FrameRotation>(turn);
    geometry.oriented_size = {turn % 2 ? source_height : source_width,
                              turn % 2 ? source_width : source_height};
    return geometry;
  }

 private:
  symbian::api::display::WindowSize native_size_;
  symbian::api::display::WindowSize source_size_;
  std::optional<ViewfinderGeometry> placement_;
};

// Sample directly from producer storage into the logical software window.
// Window Server supplies its display transform; no source copy is rotated.
inline absl::Status RenderSoftwareViewfinder(
    const symbian::api::camera::FrameView& source,
    symbian::api::display::Rgb565Frame target,
    const ViewfinderGeometry& geometry,
    symbian::api::camera::ResampleFilter filter) {
  namespace camera = symbian::api::camera;
  ABSL_ASSIGN_OR_RETURN(auto fit,
                        PreviewFit(geometry.oriented_size, target.size));
  std::fill(target.pixels.begin(), target.pixels.end(), std::byte{0});
  const std::size_t offset = static_cast<std::size_t>(fit.y) *
                                 static_cast<std::size_t>(target.pitch_bytes) +
                             static_cast<std::size_t>(fit.x) * 2;
  camera::MutableFrameView output;
  output.layout = {.width = fit.width,
                   .height = fit.height,
                   .format = camera::PixelFormat::kRgb565,
                   .stride_bytes = {target.pitch_bytes, 0, 0}};
  output.planes[0] = target.pixels.subspan(offset);
  return camera::TransformFrame(source, output, filter, nullptr,
                                geometry.orientation);
}

}  // namespace camera_app

#endif  // EXAMPLES_CAMERA_APP_VIEWFINDER_PRESENTATION_H_
