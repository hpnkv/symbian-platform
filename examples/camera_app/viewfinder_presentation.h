// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef EXAMPLES_CAMERA_APP_VIEWFINDER_PRESENTATION_H_
#define EXAMPLES_CAMERA_APP_VIEWFINDER_PRESENTATION_H_

#include <algorithm>
#include <cstddef>
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

class ViewfinderPresentation final {
 public:
  explicit ViewfinderPresentation(symbian::api::display::WindowSize native_size)
      : native_size_(native_size) {}

  auto preferences() const { return CapturePreferences(); }

  absl::StatusOr<ViewfinderGeometry> Describe(
      int source_width, int source_height,
      symbian::api::display::DisplayRotation display) {
    namespace camera = symbian::api::camera;
    if (source_width <= 0 || source_height <= 0 || native_size_.width <= 0 ||
        native_size_.height <= 0) {
      return absl::InvalidArgumentError("Invalid viewfinder dimensions");
    }
    // Genuine capture renegotiation can change the source dimensions. Display
    // rotation cannot: it is deliberately absent from the placement cache key.
    if (!placement_.has_value() || source_width != source_size_.width ||
        source_height != source_size_.height) {
      // Pixel axes describe geometry, not sensor mounting. Turning an already
      // upright camera frame merely to fit the panel makes the scene sideways.
      // Keep the producer's native reference; minimize margins by cropping
      // overflow without enlarging source pixels.
      const symbian::api::display::WindowSize available{
          std::min(source_width, native_size_.width),
          std::min(source_height, native_size_.height)};
      ABSL_ASSIGN_OR_RETURN(auto crop, camera::CenteredAspectCrop(
                                           source_width, source_height,
                                           available.width, available.height));
      const camera::FrameRect viewport{
          (native_size_.width - available.width) / 2,
          (native_size_.height - available.height) / 2, available.width,
          available.height};
      source_size_ = {source_width, source_height};
      placement_ =
          ViewfinderGeometry{.crop = crop,
                             .orientation = camera::FrameRotation::k0,
                             .oriented_size = {},
                             .native_orientation = camera::FrameRotation::k0,
                             .native_size = {crop.width, crop.height},
                             .native_viewport = viewport};
    }
    auto geometry = *placement_;
    // Metadata for consumers using logical screen coordinates. The native
    // GLES renderer uses the cached native fields, never this changing angle.
    const int turn = (static_cast<int>(geometry.native_orientation) -
                      static_cast<int>(display) + 4) %
                     4;
    geometry.orientation = static_cast<camera::FrameRotation>(turn);
    geometry.oriented_size = {
        turn % 2 ? geometry.crop.height : geometry.crop.width,
        turn % 2 ? geometry.crop.width : geometry.crop.height};
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
  const bool turned = static_cast<int>(geometry.orientation) % 2 != 0;
  const int width =
      turned ? geometry.native_viewport.height : geometry.native_viewport.width;
  const int height =
      turned ? geometry.native_viewport.width : geometry.native_viewport.height;
  if (width <= 0 || height <= 0 || width > target.size.width ||
      height > target.size.height) {
    return absl::InvalidArgumentError("Preview exceeds software target");
  }
  const camera::FrameRect fit{(target.size.width - width) / 2,
                              (target.size.height - height) / 2, width, height};
  ABSL_ASSIGN_OR_RETURN(auto cropped,
                        camera::CropFrameView(source, geometry.crop));
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
  return camera::TransformFrame(cropped, output, filter, nullptr,
                                geometry.orientation);
}

}  // namespace camera_app

#endif  // EXAMPLES_CAMERA_APP_VIEWFINDER_PRESENTATION_H_
