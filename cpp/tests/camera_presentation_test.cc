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

TEST(CameraPresentation,
     PhoneRearCropAndStorageStayFixedThroughAllOrientations) {
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
      EXPECT_EQ(geometry->crop.x, 139);
      EXPECT_EQ(geometry->crop.y, 0);
      EXPECT_EQ(geometry->crop.width, 202);
      EXPECT_EQ(geometry->crop.height, 360);
      EXPECT_EQ(geometry->orientation,
                expected[static_cast<std::size_t>(turn)]);
      EXPECT_EQ(geometry->oriented_size.width, turn % 2 == 0 ? 202 : 360);
      EXPECT_EQ(geometry->oriented_size.height, turn % 2 == 0 ? 360 : 202);
      const auto borrowed = camera::CropFrameView(frame, geometry->crop);
      ASSERT_TRUE(borrowed.ok()) << borrowed.status();
      EXPECT_EQ(borrowed->planes[0].data(), pixels.data() + 139);
      EXPECT_EQ(borrowed->layout.width, 202);
      EXPECT_EQ(borrowed->layout.height, 360);
      EXPECT_EQ(borrowed->layout.stride_bytes[0], 480);
    }
  }
  EXPECT_EQ(pixels, std::vector<std::byte>(480 * 360, std::byte{123}));
}

TEST(CameraPresentation, ModesFillTheScreenWithDistinctOrientationPolicies) {
  ViewfinderPresentation view({360, 640});
  view.Configure(ViewfinderMode::kRotateView, {360, 640});
  EXPECT_EQ(view.preferences()[0].width, 360);
  EXPECT_EQ(view.preferences()[0].height, 640);
  // Measured 808 fallback. Portrait filling necessarily retains a centered
  // strip; landscape must expose the full width rather than keep that strip.
  const auto portrait = view.Describe(360, 270, display::DisplayRotation::k0);
  ASSERT_TRUE(portrait.ok());
  EXPECT_EQ(portrait->crop.x, 104);
  EXPECT_EQ(portrait->crop.y, 0);
  EXPECT_EQ(portrait->crop.width, 151);
  EXPECT_EQ(portrait->crop.height, 270);
  for (int turn = 0; turn < 4; ++turn) {
    const auto image =
        view.Describe(480, 360, static_cast<display::DisplayRotation>(turn));
    ASSERT_TRUE(image.ok());
    EXPECT_EQ(image->orientation, camera::FrameRotation::k0);
    EXPECT_EQ(image->crop.width, turn % 2 ? 480 : 202);
    EXPECT_EQ(image->crop.height, turn % 2 ? 270 : 360);
    EXPECT_EQ(image->crop.x, turn % 2 ? 0 : 139);
    EXPECT_EQ(image->crop.y, turn % 2 ? 45 : 0);
  }
}

TEST(CameraPresentation, OpeningInLandscapeDoesNotInferSensorMounting) {
  ViewfinderPresentation view({640, 360});
  const auto rear = view.Describe(480, 360, display::DisplayRotation::k90);
  ASSERT_TRUE(rear.ok());
  EXPECT_EQ(rear->orientation, camera::FrameRotation::kClockwise270);
  EXPECT_EQ(rear->crop.width, 202);
  EXPECT_EQ(rear->crop.height, 360);
  view.Configure(ViewfinderMode::kRotateView, {640, 360});
  const auto rotate = view.Describe(480, 360, display::DisplayRotation::k90);
  ASSERT_TRUE(rotate.ok());
  EXPECT_EQ(rotate->orientation, camera::FrameRotation::k0);
  EXPECT_EQ(rotate->crop.width, 480);
  EXPECT_EQ(rotate->crop.height, 270);
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
      const bool side_marker = (x < 8 || x >= 472) && y >= 132 && y < 228;
      const unsigned short color =
          square ? 0xffe0
                 : (side_marker ? 0x07ff
                                : kCorners[static_cast<std::size_t>(
                                      (y >= 180) * 2 + (x >= 240))]);
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
     SoftwareRearFillsGlassKeepsSquaresAndPhysicalPosition) {
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
    EXPECT_EQ(at(0, 0), 0xf800);
    EXPECT_EQ(at(359, 0), 0x07e0);
    EXPECT_EQ(at(0, 639), 0x001f);
    EXPECT_EQ(at(359, 639), 0xffff);
    // All screen pixels are image content, including the former margins.
    // Integer source bounds cause at most one pixel of sampling rounding.
    int min_x = 360;
    int min_y = 640;
    int max_x = -1;
    int max_y = -1;
    for (int y = 0; y < 640; ++y) {
      for (int x = 0; x < 360; ++x) {
        ASSERT_NE(at(x, y), 0);
        if (at(x, y) == 0xffe0) {
          min_x = std::min(min_x, x);
          min_y = std::min(min_y, y);
          max_x = std::max(max_x, x);
          max_y = std::max(max_y, y);
        }
      }
    }
    EXPECT_NEAR(max_x - min_x + 1, max_y - min_y + 1, 1);
    EXPECT_NEAR(max_y - min_y + 1, 96 * 640.0 / 360, 1);
    EXPECT_NEAR((min_x + max_x) / 2.0, 179.5, 1);
    EXPECT_NEAR((min_y + max_y) / 2.0, 319.5, 1);
  }
  EXPECT_EQ(pixels, original);
}

TEST(CameraPresentation, SoftwareRotateFollowsUiAndRevealsFullLandscapeWidth) {
  std::vector<std::byte> pixels((480 * 2 + 8) * 360);
  const auto source = CalibrationFrame(pixels);
  const auto original = pixels;
  ViewfinderPresentation view({360, 640});
  view.Configure(ViewfinderMode::kRotateView, {360, 640});
  for (int turn = 0; turn < 4; ++turn) {
    const display::WindowSize size = turn % 2 ? display::WindowSize{640, 360}
                                              : display::WindowSize{360, 640};
    const auto geometry =
        view.Describe(480, 360, static_cast<display::DisplayRotation>(turn));
    ASSERT_TRUE(geometry.ok());
    std::vector<std::byte> output(
        static_cast<std::size_t>(size.width * size.height * 2));
    ASSERT_TRUE(RenderSoftwareViewfinder(source, {output, size.width * 2, size},
                                         *geometry,
                                         camera::ResampleFilter::kNearest)
                    .ok());
    int yellow = 0;
    int side_markers = 0;
    int min_x = size.width;
    int min_y = size.height;
    int max_x = -1;
    int max_y = -1;
    for (std::size_t index = 0; index < output.size(); index += 2) {
      const auto color =
          std::to_integer<unsigned int>(output[index]) |
          (std::to_integer<unsigned int>(output[index + 1]) << 8);
      ASSERT_NE(color, 0);
      side_markers += color == 0x07ff;
      if (color == 0xffe0) {
        ++yellow;
        const int x = static_cast<int>(index / 2) % size.width;
        const int y = static_cast<int>(index / 2) / size.width;
        min_x = std::min(min_x, x);
        min_y = std::min(min_y, y);
        max_x = std::max(max_x, x);
        max_y = std::max(max_y, y);
      }
    }
    // The central 96x96 square uses the full source height in portrait and
    // full width in landscape. Turning the UI changes framing, unlike Rear.
    const double scale = turn % 2 ? 640.0 / 480 : 640.0 / 360;
    EXPECT_NEAR(yellow, 96 * scale * 96 * scale, 400);
    EXPECT_NEAR(max_x - min_x + 1, max_y - min_y + 1, 1);
    EXPECT_EQ(side_markers > 0, turn % 2 != 0);
    EXPECT_EQ(std::to_integer<unsigned int>(output[0]), 0);
    EXPECT_EQ(std::to_integer<unsigned int>(output[1]), 0xf8);
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
