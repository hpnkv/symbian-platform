// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef EXAMPLES_CAMERA_APP_VIEWFINDER_PRESENTATION_H_
#define EXAMPLES_CAMERA_APP_VIEWFINDER_PRESENTATION_H_

#include <algorithm>
#include <optional>

#include "capture_preferences.h"

namespace camera_app {

enum class ViewfinderMode { kRearCamera, kRotateView };

// Pixels and crop remain in sensor order. Consumers that save/transfer a frame
// carry orientation separately, instead of rotating or resizing its storage.
struct ViewfinderGeometry {
  symbian::api::camera::FrameRect crop;
  symbian::api::camera::FrameRotation orientation;
  symbian::api::display::WindowSize oriented_size;
};

class ViewfinderPresentation final {
 public:
  ViewfinderPresentation(symbian::api::display::DisplayRotation display,
                         symbian::api::display::WindowSize size) {
    Configure(ViewfinderMode::kRearCamera, display, size);
  }

  void Configure(ViewfinderMode mode,
                 symbian::api::display::DisplayRotation display,
                 symbian::api::display::WindowSize size) {
    mode_ = mode;
    // A rear sensor is mounted in the device's normal landscape orientation,
    // regardless of which orientation the UI had when the camera was opened.
    reference_ = mode == ViewfinderMode::kRearCamera
                     ? symbian::api::display::DisplayRotation::k90
                     : display;
    target_ = mode == ViewfinderMode::kRearCamera
                  ? symbian::api::display::WindowSize{std::max(size.width,
                                                               size.height),
                                                      std::min(size.width,
                                                               size.height)}
                  : size;
    crop_.reset();
  }

  ViewfinderMode mode() const { return mode_; }

  auto preferences() const {
    return CapturePreferences(target_.height > target_.width);
  }

  absl::StatusOr<ViewfinderGeometry> Describe(
      int source_width, int source_height,
      symbian::api::display::DisplayRotation display) {
    namespace camera = symbian::api::camera;
    // Orientation alone never invalidates the chosen crop. A newly negotiated
    // source size or an explicit mode change may require a new one.
    if (!crop_ || source_width != source_width_ ||
        source_height != source_height_) {
      auto crop = camera::CenteredAspectCrop(source_width, source_height,
                                             target_.width, target_.height);
      if (!crop.ok()) {
        return crop.status();
      }
      crop_ = *crop;
      source_width_ = source_width;
      source_height_ = source_height;
    }
    const auto orientation = PresentationRotation(display, reference_);
    const bool quarter_turn =
        orientation == camera::FrameRotation::kClockwise90 ||
        orientation == camera::FrameRotation::kClockwise270;
    return ViewfinderGeometry{
        .crop = *crop_,
        .orientation = orientation,
        .oriented_size = {quarter_turn ? crop_->height : crop_->width,
                          quarter_turn ? crop_->width : crop_->height}};
  }

 private:
  ViewfinderMode mode_ = ViewfinderMode::kRearCamera;
  symbian::api::display::DisplayRotation reference_;
  symbian::api::display::WindowSize target_;
  int source_width_ = 0;
  int source_height_ = 0;
  std::optional<symbian::api::camera::FrameRect> crop_;
};

}  // namespace camera_app

#endif  // EXAMPLES_CAMERA_APP_VIEWFINDER_PRESENTATION_H_
