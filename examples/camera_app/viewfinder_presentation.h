// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef EXAMPLES_CAMERA_APP_VIEWFINDER_PRESENTATION_H_
#define EXAMPLES_CAMERA_APP_VIEWFINDER_PRESENTATION_H_

#include <algorithm>

#include <absl/status/status_macros.h>

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
    native_size_ = {std::min(size.width, size.height),
                    std::max(size.width, size.height)};
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
    // Rear cancels Window Server rotation, keeping both sampling and the
    // centered crop fixed on the physical glass. Rotate follows the logical
    // UI instead: no inverse rotation, and a crop for its current aspect.
    // Neither policy infers sensor mounting from ECam's requested dimensions.
    const bool display_quarter_turn = static_cast<int>(display) % 2 != 0;
    const auto orientation =
        mode_ == ViewfinderMode::kRearCamera
            ? PresentationRotation(display,
                                   symbian::api::display::DisplayRotation::k0)
            : camera::FrameRotation::k0;
    const bool quarter_turn =
        orientation == camera::FrameRotation::kClockwise90 ||
        orientation == camera::FrameRotation::kClockwise270;
    auto crop_size = native_size_;
    if (mode_ == ViewfinderMode::kRotateView && display_quarter_turn) {
      std::swap(crop_size.width, crop_size.height);
    }
    ABSL_ASSIGN_OR_RETURN(auto crop, camera::CenteredAspectCrop(
                                         source_width, source_height,
                                         crop_size.width, crop_size.height));
    return ViewfinderGeometry{
        .crop = crop,
        .orientation = orientation,
        .oriented_size = {quarter_turn ? crop.height : crop.width,
                          quarter_turn ? crop.width : crop.height}};
  }

 private:
  ViewfinderMode mode_ = ViewfinderMode::kRearCamera;
  symbian::api::display::WindowSize target_;
  symbian::api::display::WindowSize native_size_;
};

// Borrow the centered source rectangle; render directly into screen storage.
// No rotated/recropped source allocation or second camera crop is needed.
inline absl::Status RenderSoftwareViewfinder(
    const symbian::api::camera::FrameView& source,
    symbian::api::display::Rgb565Frame target,
    const ViewfinderGeometry& geometry,
    symbian::api::camera::ResampleFilter filter) {
  namespace camera = symbian::api::camera;
  ABSL_ASSIGN_OR_RETURN(auto cropped,
                        camera::CropFrameView(source, geometry.crop));
  camera::MutableFrameView output;
  output.layout = {.width = target.size.width,
                   .height = target.size.height,
                   .format = camera::PixelFormat::kRgb565,
                   .stride_bytes = {target.pitch_bytes, 0, 0}};
  output.planes[0] = target.pixels;
  return camera::TransformFrame(cropped, output, filter, nullptr,
                                geometry.orientation);
}

}  // namespace camera_app

#endif  // EXAMPLES_CAMERA_APP_VIEWFINDER_PRESENTATION_H_
