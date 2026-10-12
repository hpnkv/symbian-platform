// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include <array>
#include <cstddef>
#include <cstdlib>
#include <span>
#include <vector>

#include <gtest/gtest.h>

#include "preview_controls.h"
#include "viewfinder_presentation.h"

namespace camera_app {
namespace {

namespace camera = symbian::api::camera;
namespace display = symbian::api::display;

TEST(CameraPresentation,
     RequestsFullFourByThreeSceneInEitherOpeningOrientation) {
  for (auto size :
       {display::WindowSize{360, 640}, display::WindowSize{640, 360}}) {
    ViewfinderPresentation view(size);
    EXPECT_EQ(view.preferences()[0].width, 640);
    EXPECT_EQ(view.preferences()[0].height, 480);
  }
}

TEST(CameraPresentation, UprightReferenceAndCropStayFixed) {
  ViewfinderPresentation view({360, 640});
  for (int turn = 0; turn < 4; ++turn) {
    const auto geometry =
        view.Describe(480, 360, static_cast<display::DisplayRotation>(turn));
    ASSERT_TRUE(geometry.ok()) << geometry.status();
    EXPECT_EQ(geometry->crop.x, 60);
    EXPECT_EQ(geometry->crop.y, 0);
    EXPECT_EQ(geometry->crop.width, 360);
    EXPECT_EQ(geometry->crop.height, 360);
    EXPECT_EQ(geometry->native_orientation, camera::FrameRotation::k0);
    EXPECT_EQ(geometry->native_viewport.x, 0);
    EXPECT_EQ(geometry->native_viewport.y, 140);
    EXPECT_EQ(geometry->native_viewport.width, 360);
    EXPECT_EQ(geometry->native_viewport.height, 360);
    EXPECT_EQ(static_cast<int>(geometry->orientation), (4 - turn) % 4);
  }
  EXPECT_FALSE(view.Describe(0, 360, display::DisplayRotation::k0).ok());
  EXPECT_FALSE(view.Describe(480, -1, display::DisplayRotation::k0).ok());
}

TEST(CameraPresentation, MinimizesMarginsWithoutTurningTheSceneOrEnlargement) {
  ViewfinderPresentation portrait({360, 640});
  auto small = portrait.Describe(320, 240, display::DisplayRotation::k0);
  ASSERT_TRUE(small.ok());
  // Smaller frames keep their original scale and axes.
  EXPECT_EQ(small->native_orientation, camera::FrameRotation::k0);
  EXPECT_EQ(small->native_viewport.width, 320);
  EXPECT_EQ(small->native_viewport.height, 240);
  auto tall = portrait.Describe(480, 640, display::DisplayRotation::k0);
  ASSERT_TRUE(tall.ok());
  EXPECT_EQ(tall->native_orientation, camera::FrameRotation::k0);
  EXPECT_EQ(tall->native_viewport.width, 360);
  EXPECT_EQ(tall->native_viewport.height, 640);
  // The phone now delivers 640x480. Keep all 480 rows, crop only the
  // horizontal overflow, and preserve the 360x480 footprint of 1.0.30.
  auto phone = portrait.Describe(640, 480, display::DisplayRotation::k0);
  ASSERT_TRUE(phone.ok());
  EXPECT_EQ(phone->native_orientation, camera::FrameRotation::k0);
  EXPECT_EQ(phone->crop.x, 140);
  EXPECT_EQ(phone->crop.width, 360);
  EXPECT_EQ(phone->crop.height, 480);
  EXPECT_EQ(phone->native_viewport.y, 80);
  EXPECT_EQ(phone->native_viewport.width, 360);
  EXPECT_EQ(phone->native_viewport.height, 480);
  ViewfinderPresentation landscape({640, 360});
  auto wide = landscape.Describe(480, 360, display::DisplayRotation::k0);
  ASSERT_TRUE(wide.ok());
  EXPECT_EQ(wide->native_orientation, camera::FrameRotation::k0);
  EXPECT_EQ(wide->native_viewport.width, 480);
  EXPECT_EQ(wide->native_viewport.height, 360);
}

// Four different corner colors and a centered square reveal quarter-turns
// and stretched proportions without relying on a live scene.
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

TEST(CameraPresentation, CropKeepsSceneUpSquaresAndPhysicalPosition) {
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
    // Producer top remains physical top: fitting the source's long axis to
    // the panel's long axis would instead put blue at this top-left corner.
    EXPECT_EQ(at(0, 140), 0xf800);
    EXPECT_EQ(at(359, 140), 0x07e0);
    EXPECT_EQ(at(0, 499), 0x001f);
    EXPECT_EQ(at(359, 499), 0xffff);
    int min_x = 360;
    int min_y = 640;
    int max_x = -1;
    int max_y = -1;
    for (int y = 0; y < 640; ++y) {
      for (int x = 0; x < 360; ++x) {
        if (y < 140 || y >= 500) {
          EXPECT_EQ(at(x, y), 0);
        } else {
          ASSERT_NE(at(x, y), 0);
        }
        if (at(x, y) == 0xffe0) {
          min_x = std::min(min_x, x);
          min_y = std::min(min_y, y);
          max_x = std::max(max_x, x);
          max_y = std::max(max_y, y);
        }
      }
    }
    EXPECT_NEAR(max_x - min_x + 1, max_y - min_y + 1, 1);
    EXPECT_NEAR(max_y - min_y + 1, 96, 1);
    EXPECT_NEAR((min_x + max_x) / 2.0, 179.5, 1);
    EXPECT_NEAR((min_y + max_y) / 2.0, 319.5, 1);
  }
  EXPECT_EQ(pixels, original);
}

TEST(CameraPresentation, SmoothingControlRotatesAroundOnePhysicalAnchor) {
  for (int turn = 0; turn < 4; ++turn) {
    const display::WindowSize size = turn % 2 ? display::WindowSize{640, 360}
                                              : display::WindowSize{360, 640};
    const auto rotation = static_cast<display::DisplayRotation>(turn);
    const auto logical = PreviewButton(size, rotation);
    const auto native = NativePreviewRect(logical, size, rotation);
    EXPECT_EQ(native.x + native.width / 2, 180);
    EXPECT_EQ(native.y + native.height / 2, 584);
    EXPECT_EQ(native.width, turn % 2 ? 36 : 88);
    EXPECT_EQ(native.height, turn % 2 ? 88 : 36);
    for (bool smooth : {false, true}) {
      int rectangles = 0;
      DrawPreviewButton(
          size, rotation, smooth,
          [&](int x, int y, int width, int height, std::uint32_t color) {
            const auto rect =
                NativePreviewRect({x, y, width, height}, size, rotation);
            EXPECT_GE(rect.x, native.x);
            EXPECT_GE(rect.y, native.y);
            EXPECT_LE(rect.x + rect.width, native.x + native.width);
            EXPECT_LE(rect.y + rect.height, native.y + native.height);
            EXPECT_NE(color, 0);
            ++rectangles;
          });
      EXPECT_GT(rectangles, 100);
    }
  }
}

}  // namespace
}  // namespace camera_app
