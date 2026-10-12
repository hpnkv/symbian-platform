// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include <array>
#include <cstddef>
#include <cstdlib>
#include <span>
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
    ViewfinderPresentation view(size);
    EXPECT_EQ(view.mode(), ViewfinderMode::kRearCamera);
    EXPECT_EQ(view.preferences()[0].width, 640);
    EXPECT_EQ(view.preferences()[0].height, 360);
  }
}

TEST(CameraPresentation, PhoneRearStorageAndEdgesSurviveAllOrientations) {
  ViewfinderPresentation view({360, 640});
  std::vector<std::byte> pixels(480 * 360, std::byte{123});
  camera::FrameView frame;
  frame.layout = {.width = 480,
                  .height = 360,
                  .format = camera::PixelFormat::kGray8,
                  .stride_bytes = {480, 0, 0}};
  frame.planes[0] = pixels;
  const std::array<camera::FrameRotation, 4> expected{
      camera::FrameRotation::k0, camera::FrameRotation::kClockwise270,
      camera::FrameRotation::k180, camera::FrameRotation::kClockwise90};
  for (int round = 0; round < 3; ++round) {
    for (int turn = 0; turn < 4; ++turn) {
      const auto geometry =
          view.Describe(480, 360, static_cast<display::DisplayRotation>(turn));
      ASSERT_TRUE(geometry.ok()) << geometry.status();
      EXPECT_EQ(geometry->crop.x, 0);
      EXPECT_EQ(geometry->crop.y, 0);
      EXPECT_EQ(geometry->crop.width, 480);
      EXPECT_EQ(geometry->crop.height, 360);
      EXPECT_EQ(geometry->orientation,
                expected[static_cast<std::size_t>(turn)]);
      EXPECT_EQ(geometry->oriented_size.width, turn % 2 == 0 ? 480 : 360);
      EXPECT_EQ(geometry->oriented_size.height, turn % 2 == 0 ? 360 : 480);
      const auto borrowed = camera::CropFrameView(frame, geometry->crop);
      ASSERT_TRUE(borrowed.ok()) << borrowed.status();
      EXPECT_EQ(borrowed->planes[0].data(), pixels.data());
      EXPECT_EQ(borrowed->layout.width, 480);
      EXPECT_EQ(borrowed->layout.height, 360);
      EXPECT_EQ(borrowed->layout.stride_bytes[0], 480);
    }
  }
  EXPECT_EQ(pixels, std::vector<std::byte>(480 * 360, std::byte{123}));
}

TEST(CameraPresentation,
     ExplicitModeChangeRequestsShapeWithoutCroppingFallback) {
  ViewfinderPresentation view({360, 640});
  view.Configure(ViewfinderMode::kRotateView, {360, 640});
  EXPECT_EQ(view.preferences()[0].width, 360);
  EXPECT_EQ(view.preferences()[0].height, 640);
  // Actual 808 fallback from the 1.0.27 phone trace. It previously kept only
  // 151 of 360 columns and magnified them across the whole portrait display.
  const auto rotated = view.Describe(360, 270, display::DisplayRotation::k0);
  ASSERT_TRUE(rotated.ok());
  EXPECT_EQ(rotated->crop.x, 0);
  EXPECT_EQ(rotated->crop.y, 0);
  EXPECT_EQ(rotated->crop.width, 360);
  EXPECT_EQ(rotated->crop.height, 270);
  EXPECT_EQ(rotated->orientation, camera::FrameRotation::k0);
  view.Configure(ViewfinderMode::kRearCamera, {360, 640});
  const auto rear = view.Describe(480, 360, display::DisplayRotation::k0);
  ASSERT_TRUE(rear.ok());
  EXPECT_EQ(rear->crop.x, 0);
  EXPECT_EQ(rear->crop.y, 0);
  EXPECT_EQ(rear->crop.width, 480);
  EXPECT_EQ(rear->crop.height, 360);
  EXPECT_EQ(rear->orientation, camera::FrameRotation::k0);
}

TEST(CameraPresentation, OpeningInLandscapeDoesNotRotateTheDeliveredBitmap) {
  ViewfinderPresentation view({640, 360});
  for (auto mode : {ViewfinderMode::kRearCamera, ViewfinderMode::kRotateView}) {
    view.Configure(mode, {640, 360});
    const auto image = view.Describe(480, 360, display::DisplayRotation::k90);
    ASSERT_TRUE(image.ok());
    EXPECT_EQ(image->orientation, camera::FrameRotation::kClockwise270);
    EXPECT_EQ(image->crop.width, 480);
    EXPECT_EQ(image->crop.height, 360);
  }
  EXPECT_FALSE(view.Describe(0, 360, display::DisplayRotation::k0).ok());
  EXPECT_FALSE(view.Describe(480, -1, display::DisplayRotation::k0).ok());
}

// Four different corner colors and a centered square reveal lost edges,
// quarter-turns and stretched proportions without relying on a live scene.
camera::FrameView CalibrationFrame(std::span<std::byte> pixels) {
  constexpr int kWidth = 480;
  constexpr int kHeight = 360;
  constexpr int kStride = kWidth * 2 + 8;
  constexpr std::array<unsigned short, 4> kCorners{0xf800, 0x07e0, 0x001f,
                                                   0xffff};
  for (int y = 0; y < kHeight; ++y) {
    for (int x = 0; x < kWidth; ++x) {
      const bool square = x >= 192 && x < 288 && y >= 132 && y < 228;
      const unsigned short color =
          square
              ? 0xffe0
              : kCorners[static_cast<std::size_t>((y >= 180) * 2 + (x >= 240))];
      const auto offset = static_cast<std::size_t>(y * kStride + x * 2);
      pixels[offset] = static_cast<std::byte>(color & 255);
      pixels[offset + 1] = static_cast<std::byte>(color >> 8);
    }
  }
  camera::FrameView frame;
  frame.layout = {.width = kWidth,
                  .height = kHeight,
                  .format = camera::PixelFormat::kRgb565,
                  .stride_bytes = {kStride, 0, 0}};
  frame.planes[0] = pixels;
  return frame;
}

TEST(CameraPresentation,
     SoftwarePhonePreviewKeepsCornersSquareAndPhysicalPosition) {
  std::vector<std::byte> pixels((480 * 2 + 8) * 360);
  const auto source = CalibrationFrame(pixels);
  const auto original = pixels;
  ViewfinderPresentation view({360, 640});
  std::vector<std::byte> baseline(360 * 640 * 2);
  for (int turn = 0; turn < 4; ++turn) {
    const display::WindowSize size = turn % 2 ? display::WindowSize{640, 360}
                                              : display::WindowSize{360, 640};
    const auto rotation = static_cast<display::DisplayRotation>(turn);
    const auto geometry = view.Describe(480, 360, rotation);
    ASSERT_TRUE(geometry.ok());
    std::vector<std::byte> output(
        static_cast<std::size_t>(size.width * size.height * 2), std::byte{77});
    ASSERT_TRUE(RenderSoftwareViewfinder(source, {output, size.width * 2, size},
                                         *geometry,
                                         camera::ResampleFilter::kNearest)
                    .ok());
    camera::FrameView screen;
    screen.layout = {.width = size.width,
                     .height = size.height,
                     .format = camera::PixelFormat::kRgb565,
                     .stride_bytes = {size.width * 2, 0, 0}};
    screen.planes[0] = output;
    std::vector<std::byte> physical(360 * 640 * 2);
    camera::MutableFrameView glass;
    glass.layout = {.width = 360,
                    .height = 640,
                    .format = camera::PixelFormat::kRgb565,
                    .stride_bytes = {360 * 2, 0, 0}};
    glass.planes[0] = physical;
    ASSERT_TRUE(camera::TransformFrame(
                    screen, glass, camera::ResampleFilter::kNearest, nullptr,
                    static_cast<camera::FrameRotation>(turn))
                    .ok());
    if (turn == 0) {
      baseline = physical;
    } else {
      EXPECT_EQ(physical, baseline);
    }
    const auto at = [&](int x, int y) {
      const auto offset = static_cast<std::size_t>((y * 360 + x) * 2);
      return std::to_integer<unsigned int>(physical[offset]) |
             (std::to_integer<unsigned int>(physical[offset + 1]) << 8);
    };
    EXPECT_EQ(at(0, 0), 0);
    EXPECT_EQ(at(0, 184), 0);
    EXPECT_EQ(at(0, 185), 0xf800);
    EXPECT_EQ(at(359, 185), 0x07e0);
    EXPECT_EQ(at(0, 454), 0x001f);
    EXPECT_EQ(at(359, 454), 0xffff);
    EXPECT_EQ(at(0, 455), 0);
    // The 96x96 source square is 72x72 after uniform 0.75 scaling.
    int min_x = 360;
    int min_y = 640;
    int max_x = -1;
    int max_y = -1;
    for (int y = 0; y < 640; ++y) {
      for (int x = 0; x < 360; ++x) {
        if (at(x, y) == 0xffe0) {
          min_x = std::min(min_x, x);
          min_y = std::min(min_y, y);
          max_x = std::max(max_x, x);
          max_y = std::max(max_y, y);
        }
      }
    }
    EXPECT_EQ(max_x - min_x + 1, 72);
    EXPECT_EQ(max_y - min_y + 1, 72);
    EXPECT_EQ(min_x, 144);
    EXPECT_EQ(min_y, 284);
  }
  EXPECT_EQ(pixels, original);
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
