// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include <array>
#include <cstddef>
#include <cstdlib>
#include <vector>

#include <gtest/gtest.h>

#include "mode_controls.h"
#include "viewfinder_presentation.h"

namespace camera_app {
namespace {

namespace camera = symbian::api::camera;
namespace display = symbian::api::display;

TEST(CameraPresentation,
     RearDefaultRequestsLandscapeInEitherOpeningOrientation) {
  for (auto size :
       {display::WindowSize{360, 640}, display::WindowSize{640, 360}}) {
    ViewfinderPresentation view(display::DisplayRotation::k0, size);
    EXPECT_EQ(view.mode(), ViewfinderMode::kRearCamera);
    EXPECT_EQ(view.preferences()[0].width, 640);
    EXPECT_EQ(view.preferences()[0].height, 360);
  }
}

TEST(CameraPresentation, RearCropAndBorrowedStorageSurviveAllOrientations) {
  ViewfinderPresentation view(display::DisplayRotation::k0, {360, 640});
  std::vector<std::byte> pixels(640 * 480, std::byte{123});
  camera::FrameView frame;
  frame.layout = {.width = 640,
                  .height = 480,
                  .format = camera::PixelFormat::kGray8,
                  .stride_bytes = {640, 0, 0}};
  frame.planes[0] = pixels;
  const std::array<camera::FrameRotation, 4> expected{
      camera::FrameRotation::kClockwise90, camera::FrameRotation::k0,
      camera::FrameRotation::kClockwise270, camera::FrameRotation::k180};
  for (int round = 0; round < 3; ++round) {
    for (int turn = 0; turn < 4; ++turn) {
      const auto geometry =
          view.Describe(640, 480, static_cast<display::DisplayRotation>(turn));
      ASSERT_TRUE(geometry.ok()) << geometry.status();
      EXPECT_EQ(geometry->crop.x, 0);
      EXPECT_EQ(geometry->crop.y, 60);
      EXPECT_EQ(geometry->crop.width, 640);
      EXPECT_EQ(geometry->crop.height, 360);
      EXPECT_EQ(geometry->orientation,
                expected[static_cast<std::size_t>(turn)]);
      EXPECT_EQ(geometry->oriented_size.width, turn % 2 == 0 ? 360 : 640);
      EXPECT_EQ(geometry->oriented_size.height, turn % 2 == 0 ? 640 : 360);
      const auto borrowed = camera::CropFrameView(frame, geometry->crop);
      ASSERT_TRUE(borrowed.ok()) << borrowed.status();
      EXPECT_EQ(borrowed->planes[0].data(), pixels.data() + 60 * 640);
      EXPECT_EQ(borrowed->layout.width, 640);
      EXPECT_EQ(borrowed->layout.height, 360);
      EXPECT_EQ(borrowed->layout.stride_bytes[0], 640);
      EXPECT_EQ(frame.planes[0].data(), pixels.data());
      EXPECT_EQ(frame.layout.width, 640);
      EXPECT_EQ(frame.layout.height, 480);
    }
  }
  EXPECT_EQ(pixels, std::vector<std::byte>(640 * 480, std::byte{123}));
}

TEST(CameraPresentation, ExplicitModeChangeSelectsItsOwnCropAndCaptureRequest) {
  ViewfinderPresentation view(display::DisplayRotation::k0, {360, 640});
  view.Configure(ViewfinderMode::kRotateView, display::DisplayRotation::k0,
                 {360, 640});
  EXPECT_EQ(view.preferences()[0].width, 360);
  EXPECT_EQ(view.preferences()[0].height, 640);
  const auto rotated = view.Describe(640, 480, display::DisplayRotation::k0);
  ASSERT_TRUE(rotated.ok());
  EXPECT_EQ(rotated->crop.x, 185);
  EXPECT_EQ(rotated->crop.y, 0);
  EXPECT_EQ(rotated->crop.width, 270);
  EXPECT_EQ(rotated->crop.height, 480);
  EXPECT_EQ(rotated->orientation, camera::FrameRotation::k0);
  view.Configure(ViewfinderMode::kRearCamera, display::DisplayRotation::k0,
                 {360, 640});
  const auto rear = view.Describe(640, 480, display::DisplayRotation::k0);
  ASSERT_TRUE(rear.ok());
  EXPECT_EQ(rear->crop.x, 0);
  EXPECT_EQ(rear->crop.y, 60);
  EXPECT_EQ(rear->crop.width, 640);
  EXPECT_EQ(rear->crop.height, 360);
}

TEST(CameraPresentation, ButtonHitRegionsSelectModesAndIgnoreOutside) {
  for (auto size :
       {display::WindowSize{360, 640}, display::WindowSize{640, 360},
        display::WindowSize{208, 320}}) {
    const auto buttons = ModeButtons(size, display::DisplayRotation::k0);
    EXPECT_EQ(HitModeButton(size, display::DisplayRotation::k0,
                            buttons[0].x + buttons[0].width / 2,
                            buttons[0].y + buttons[0].height / 2),
              ViewfinderMode::kRearCamera);
    EXPECT_EQ(HitModeButton(size, display::DisplayRotation::k0,
                            buttons[1].x + buttons[1].width / 2,
                            buttons[1].y + buttons[1].height / 2),
              ViewfinderMode::kRotateView);
    EXPECT_FALSE(HitModeButton(size, display::DisplayRotation::k0, 0, 0));
    EXPECT_FALSE(HitModeButton(size, display::DisplayRotation::k0, size.width,
                               size.height));
  }
}

TEST(CameraPresentation,
     ButtonGroupStaysAnchoredWithCompactSpacingAsLabelsRotate) {
  for (int turn = 0; turn < 4; ++turn) {
    const display::WindowSize size = turn % 2 ? display::WindowSize{640, 360}
                                              : display::WindowSize{360, 640};
    const auto rotation = static_cast<display::DisplayRotation>(turn);
    const auto buttons = ModeButtons(size, rotation);
    if (turn % 2) {
      EXPECT_EQ(std::abs(buttons[1].y - buttons[0].y) - buttons[0].height, 8);
    } else {
      EXPECT_EQ(std::abs(buttons[1].x - buttons[0].x) - buttons[0].width, 8);
    }
    for (std::size_t index = 0; index < buttons.size(); ++index) {
      const auto rect = buttons[index];
      int x = rect.x + rect.width / 2;
      int y = rect.y + rect.height / 2;
      // Map each center back to the physical portrait glass. The group stays
      // centered at (180,584), with an eight-pixel gap between rotated pills.
      for (int step = 0; step < turn; ++step) {
        const int next_x = (step % 2 == 0 ? size.height : size.width) - y;
        y = x;
        x = next_x;
      }
      const int half_spacing = turn % 2 ? 22 : 48;
      EXPECT_EQ(x, 180 + (index == 0 ? -half_spacing : half_spacing));
      EXPECT_EQ(y, 584);
      EXPECT_EQ(rect.width, 88);
      EXPECT_EQ(rect.height, 36);
      EXPECT_GE(rect.x, 0);
      EXPECT_GE(rect.y, 0);
      EXPECT_LE(rect.x + rect.width, size.width);
      EXPECT_LE(rect.y + rect.height, size.height);
      EXPECT_EQ(HitModeButton(size, rotation, rect.x + 44, rect.y + 18),
                index == 0 ? ViewfinderMode::kRearCamera
                           : ViewfinderMode::kRotateView);
    }
  }
}

}  // namespace
}  // namespace camera_app
