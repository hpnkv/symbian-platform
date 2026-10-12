// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef EXAMPLES_CAMERA_APP_VIEWFINDER_PRESENTATION_H_
#define EXAMPLES_CAMERA_APP_VIEWFINDER_PRESENTATION_H_

#include <algorithm>
#include <cstddef>

#include "capture_preferences.h"

namespace camera_app {

enum class ViewfinderMode { kRearCamera, kRotateView };

// Pixels stay in producer order. Saving/transferring consumers carry
// orientation separately instead of rotating or resizing source storage.
struct ViewfinderGeometry {
  symbian::api::camera::FrameRect crop;
  symbian::api::camera::FrameRotation orientation;
  symbian::api::display::WindowSize oriented_size;
};

class ViewfinderPresentation final {
 public:
  explicit ViewfinderPresentation(symbian::api::display::WindowSize size) {
    Configure(ViewfinderMode::kRearCamera, size);
  }

  void Configure(ViewfinderMode mode, symbian::api::display::WindowSize size) {
    mode_ = mode;
    // Request dimensions do not specify pixel orientation. The phone's ECam
    // bitmap is aligned with the native display even when opened in landscape.
    target_ = mode == ViewfinderMode::kRearCamera
                  ? symbian::api::display::WindowSize{std::max(size.width,
                                                               size.height),
                                                      std::min(size.width,
                                                               size.height)}
                  : size;
  }

  ViewfinderMode mode() const { return mode_; }

  auto preferences() const {
    return CapturePreferences(target_.height > target_.width);
  }

  absl::StatusOr<ViewfinderGeometry> Describe(
      int source_width, int source_height,
      symbian::api::display::DisplayRotation display) const {
    namespace camera = symbian::api::camera;
    if (source_width <= 0 || source_height <= 0) {
      return absl::InvalidArgumentError("Invalid viewfinder dimensions");
    }
    // ECam may return 480x360 for a landscape request or 360x270 for a
    // portrait request. Keep the whole returned scene instead of inferring a
    // sensor mounting angle or cropping it to the requested dimensions.
    const auto orientation = PresentationRotation(
        display, symbian::api::display::DisplayRotation::k0);
    const bool quarter_turn =
        orientation == camera::FrameRotation::kClockwise90 ||
        orientation == camera::FrameRotation::kClockwise270;
    return ViewfinderGeometry{
        .crop = {0, 0, source_width, source_height},
        .orientation = orientation,
        .oriented_size = {quarter_turn ? source_height : source_width,
                          quarter_turn ? source_width : source_height}};
  }

 private:
  ViewfinderMode mode_ = ViewfinderMode::kRearCamera;
  symbian::api::display::WindowSize target_;
};

// The target is a valid SDK WindowSurface bitmap. Preserve every source edge
// and its proportions; clear unused bands rather than magnifying a fallback.
inline absl::Status RenderSoftwareViewfinder(
    const symbian::api::camera::FrameView& source,
    symbian::api::display::Rgb565Frame target,
    const ViewfinderGeometry& geometry,
    symbian::api::camera::ResampleFilter filter) {
  namespace camera = symbian::api::camera;
  const auto fit = camera::CenteredAspectFit(
      geometry.oriented_size.width, geometry.oriented_size.height,
      target.size.width, target.size.height);
  if (!fit.ok()) {
    return fit.status();
  }
  std::fill(target.pixels.begin(), target.pixels.end(), std::byte{0});
  const std::size_t offset = static_cast<std::size_t>(fit->y) *
                                 static_cast<std::size_t>(target.pitch_bytes) +
                             static_cast<std::size_t>(fit->x) * 2;
  camera::MutableFrameView output;
  output.layout = {.width = fit->width,
                   .height = fit->height,
                   .format = camera::PixelFormat::kRgb565,
                   .stride_bytes = {target.pitch_bytes, 0, 0}};
  output.planes[0] = target.pixels.subspan(offset);
  return camera::TransformFrame(source, output, filter, nullptr,
                                geometry.orientation);
}

}  // namespace camera_app

#endif  // EXAMPLES_CAMERA_APP_VIEWFINDER_PRESENTATION_H_
