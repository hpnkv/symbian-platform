// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef EXAMPLES_CAMERA_APP_CAPTURE_PREFERENCES_H_
#define EXAMPLES_CAMERA_APP_CAPTURE_PREFERENCES_H_

#include <array>

#include "symbian/api/camera/frame.h"
#include "symbian/api/display/window_surface.h"

namespace camera_app {

inline std::array<symbian::api::camera::FrameLayout, 8> CapturePreferences(
    bool portrait) {
  namespace camera = symbian::api::camera;
  const std::array<std::array<int, 2>, 4> sizes =
      portrait ? std::array<std::array<int, 2>, 4>{
                     {{480, 640}, {360, 640}, {640, 480}, {320, 240}}}
               : std::array<std::array<int, 2>, 4>{
                     {{640, 360}, {640, 480}, {320, 240}, {320, 180}}};
  std::array<camera::FrameLayout, 8> preferences{};
  for (int i = 0; i < 4; ++i) {
    preferences[i * 2] = {.width = sizes[i][0],
                          .height = sizes[i][1],
                          .format = camera::PixelFormat::kBgr565};
    preferences[i * 2 + 1] = {.width = sizes[i][0],
                              .height = sizes[i][1],
                              .format = camera::PixelFormat::kRgbx8888};
  }
  return preferences;
}

// ECam is asked for the opening display orientation once. Later display
// changes rotate sampling only; the capture mode and source storage stay put.
inline symbian::api::camera::FrameRotation PresentationRotation(
    symbian::api::display::DisplayRotation display,
    symbian::api::display::DisplayRotation opening_display) {
  namespace camera = symbian::api::camera;
  const int quarter_turns =
      (static_cast<int>(display) - static_cast<int>(opening_display) + 4) % 4;
  switch (quarter_turns) {
    case 0:
      return camera::FrameRotation::k0;
    case 1:
      return camera::FrameRotation::kClockwise270;
    case 2:
      return camera::FrameRotation::k180;
    case 3:
      return camera::FrameRotation::kClockwise90;
  }
  return camera::FrameRotation::k0;
}

}  // namespace camera_app

#endif  // EXAMPLES_CAMERA_APP_CAPTURE_PREFERENCES_H_
