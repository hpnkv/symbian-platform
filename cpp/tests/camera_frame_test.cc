// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "symbian/api/camera/frame.h"

namespace symbian::api::camera {
namespace {

FrameView Source(PixelFormat format, int width, int height, int stride,
                 std::span<const std::byte> pixels) {
  FrameView frame;
  frame.layout = {.width = width,
                  .height = height,
                  .format = format,
                  .stride_bytes = {stride, 0, 0}};
  frame.planes[0] = pixels;
  return frame;
}

MutableFrameView Destination(PixelFormat format, int width, int height,
                             int stride, std::span<std::byte> pixels) {
  MutableFrameView frame;
  frame.layout = {.width = width,
                  .height = height,
                  .format = format,
                  .stride_bytes = {stride, 0, 0}};
  frame.planes[0] = pixels;
  return frame;
}

TEST(CameraFrame, CopiesOnlyVisiblePixelsAcrossPaddedRows) {
  const std::array<std::byte, 10> source{
      std::byte{1},  std::byte{2}, std::byte{3}, std::byte{4}, std::byte{99},
      std::byte{99}, std::byte{5}, std::byte{6}, std::byte{7}, std::byte{8}};
  std::array<std::byte, 10> output{};
  const auto input = Source(PixelFormat::kRgb565, 2, 2, 6, source);
  auto destination = Destination(PixelFormat::kRgb565, 2, 2, 6, output);
  ASSERT_TRUE(
      TransformFrame(input, destination, ResampleFilter::kNearest).ok());
  EXPECT_EQ(output[0], std::byte{1});
  EXPECT_EQ(output[3], std::byte{4});
  EXPECT_EQ(output[4], std::byte{0});
  EXPECT_EQ(output[6], std::byte{5});
  EXPECT_EQ(output[7], std::byte{6});
}

TEST(CameraFrame, CenteredAspectCropPreservesLongAxisAndBorrowsPixels) {
  const auto portrait = CenteredAspectCrop(640, 480, 360, 640);
  ASSERT_TRUE(portrait.ok());
  EXPECT_EQ(portrait->x, 185);
  EXPECT_EQ(portrait->y, 0);
  EXPECT_EQ(portrait->width, 270);
  EXPECT_EQ(portrait->height, 480);
  const auto landscape = CenteredAspectCrop(640, 480, 640, 360);
  ASSERT_TRUE(landscape.ok());
  EXPECT_EQ(landscape->x, 0);
  EXPECT_EQ(landscape->y, 60);
  EXPECT_EQ(landscape->width, 640);
  EXPECT_EQ(landscape->height, 360);

  const std::array<std::byte, 16> pixels{
      std::byte{0},  std::byte{1},  std::byte{2},  std::byte{3},
      std::byte{4},  std::byte{5},  std::byte{6},  std::byte{7},
      std::byte{8},  std::byte{9},  std::byte{10}, std::byte{11},
      std::byte{12}, std::byte{13}, std::byte{14}, std::byte{15}};
  const auto source = Source(PixelFormat::kGray8, 4, 4, 4, pixels);
  const auto crop = CropFrameView(source, {1, 1, 2, 2});
  ASSERT_TRUE(crop.ok());
  EXPECT_EQ(crop->planes[0].data(), pixels.data() + 5);
  EXPECT_EQ(crop->layout.stride_bytes[0], 4);
  std::array<std::byte, 4> output{};
  auto destination = Destination(PixelFormat::kGray8, 2, 2, 2, output);
  ASSERT_TRUE(
      TransformFrame(*crop, destination, ResampleFilter::kNearest).ok());
  EXPECT_EQ(output, (std::array<std::byte, 4>{std::byte{5}, std::byte{6},
                                              std::byte{9}, std::byte{10}}));
}

TEST(CameraFrame, CenteredAspectFitPreservesCompleteFrame) {
  const auto portrait = CenteredAspectFit(640, 480, 360, 640);
  ASSERT_TRUE(portrait.ok());
  EXPECT_EQ(portrait->x, 0);
  EXPECT_EQ(portrait->y, 185);
  EXPECT_EQ(portrait->width, 360);
  EXPECT_EQ(portrait->height, 270);
  const auto landscape = CenteredAspectFit(640, 480, 640, 360);
  ASSERT_TRUE(landscape.ok());
  EXPECT_EQ(landscape->x, 80);
  EXPECT_EQ(landscape->y, 0);
  EXPECT_EQ(landscape->width, 480);
  EXPECT_EQ(landscape->height, 360);
}

TEST(CameraFrame, DetectsSymmetricBlackSideMarginsWithoutCroppingTheScene) {
  std::vector<std::byte> pixels(64 * 32 * 4);
  for (int y = 0; y < 32; ++y) {
    for (int x = 16; x < 48; ++x) {
      pixels[(y * 64 + x) * 4] = std::byte{80};
    }
  }
  const auto frame = Source(PixelFormat::kBgrx8888, 64, 32, 64 * 4, pixels);
  const FrameRect content = DetectSolidSideMargins(frame);
  EXPECT_EQ(content.x, 16);
  EXPECT_EQ(content.width, 32);
  EXPECT_EQ(content.height, 32);
  pixels[0] = std::byte{80};
  for (int y = 0; y < 32; ++y) {
    pixels[(y * 64 + 0) * 4] = std::byte{80};
  }
  EXPECT_EQ(DetectSolidSideMargins(frame).width, 64);
}

TEST(CameraFrame, BilinearRgbaUsesFourNeighbors) {
  const std::array<std::byte, 16> source{
      std::byte{0},   std::byte{0},   std::byte{0},   std::byte{255},
      std::byte{64},  std::byte{64},  std::byte{64},  std::byte{255},
      std::byte{128}, std::byte{128}, std::byte{128}, std::byte{255},
      std::byte{192}, std::byte{192}, std::byte{192}, std::byte{255}};
  std::array<std::byte, 36> output{};
  const auto input = Source(PixelFormat::kRgba8888, 2, 2, 8, source);
  auto destination = Destination(PixelFormat::kRgba8888, 3, 3, 12, output);
  ASSERT_TRUE(
      TransformFrame(input, destination, ResampleFilter::kBilinear).ok());
  EXPECT_EQ(output[16], std::byte{96});
  EXPECT_EQ(output[19], std::byte{255});
}

TEST(CameraFrame, ResamplesOddRatioAtPixelCenters) {
  const std::array<std::byte, 3> source{std::byte{0}, std::byte{60},
                                        std::byte{120}};
  std::array<std::byte, 5> output{};
  const auto input = Source(PixelFormat::kGray8, 3, 1, 3, source);
  auto destination = Destination(PixelFormat::kGray8, 5, 1, 5, output);
  ASSERT_TRUE(
      TransformFrame(input, destination, ResampleFilter::kNearest).ok());
  EXPECT_EQ(output,
            (std::array<std::byte, 5>{std::byte{0}, std::byte{0}, std::byte{60},
                                      std::byte{60}, std::byte{120}}));
  ASSERT_TRUE(
      TransformFrame(input, destination, ResampleFilter::kBilinear).ok());
  EXPECT_EQ(output, (std::array<std::byte, 5>{std::byte{0}, std::byte{24},
                                              std::byte{60}, std::byte{96},
                                              std::byte{120}}));
}

TEST(CameraFrame, RotatesLandscapeCameraIntoPortraitWithoutIntermediateFrame) {
  const std::array<std::byte, 24> pixels{
      std::byte{0}, std::byte{0}, std::byte{32},  std::byte{0},
      std::byte{0}, std::byte{0}, std::byte{64},  std::byte{0},
      std::byte{0}, std::byte{0}, std::byte{96},  std::byte{0},
      std::byte{0}, std::byte{0}, std::byte{128}, std::byte{0},
      std::byte{0}, std::byte{0}, std::byte{160}, std::byte{0},
      std::byte{0}, std::byte{0}, std::byte{192}, std::byte{0}};
  const auto source = Source(PixelFormat::kBgrx8888, 2, 3, 8, pixels);
  std::array<std::byte, 12> output{};
  auto destination = Destination(PixelFormat::kRgb565, 3, 2, 6, output);
  ASSERT_TRUE(TransformFrame(source, destination, ResampleFilter::kNearest,
                             nullptr, FrameRotation::kClockwise90)
                  .ok());
  EXPECT_EQ(output,
            (std::array<std::byte, 12>{
                std::byte{0}, std::byte{0xa0}, std::byte{0}, std::byte{0x60},
                std::byte{0}, std::byte{0x20}, std::byte{0}, std::byte{0xc0},
                std::byte{0}, std::byte{0x80}, std::byte{0}, std::byte{0x40}}));
  ASSERT_TRUE(TransformFrame(source, destination, ResampleFilter::kNearest,
                             nullptr, FrameRotation::kClockwise270)
                  .ok());
  EXPECT_EQ(output,
            (std::array<std::byte, 12>{
                std::byte{0}, std::byte{0x40}, std::byte{0}, std::byte{0x80},
                std::byte{0}, std::byte{0xc0}, std::byte{0}, std::byte{0x20},
                std::byte{0}, std::byte{0x60}, std::byte{0}, std::byte{0xa0}}));
}

TEST(CameraFrame, WidescreenCropPreservesMostLandscapeFrame) {
  const auto landscape = CenteredAspectCrop(640, 480, 640, 360);
  ASSERT_TRUE(landscape.ok());
  EXPECT_EQ(landscape->x, 0);
  EXPECT_EQ(landscape->y, 60);
  EXPECT_EQ(landscape->width, 640);
  EXPECT_EQ(landscape->height, 360);
}

TEST(CameraFrame, RotatedBilinearSamplingKeepsTheCenterAndCorners) {
  const std::array<std::byte, 4> pixels{std::byte{0}, std::byte{64},
                                        std::byte{128}, std::byte{192}};
  const auto source = Source(PixelFormat::kGray8, 2, 2, 2, pixels);
  std::array<std::byte, 18> output{};
  auto destination = Destination(PixelFormat::kRgb565, 3, 3, 6, output);
  ASSERT_TRUE(TransformFrame(source, destination, ResampleFilter::kBilinear,
                             nullptr, FrameRotation::kClockwise90)
                  .ok());
  EXPECT_EQ(output[0], std::byte{0x10});
  EXPECT_EQ(output[1], std::byte{0x84});
  EXPECT_EQ(output[8], std::byte{0x0c});
  EXPECT_EQ(output[9], std::byte{0x63});
  EXPECT_EQ(output[4], std::byte{0});
  EXPECT_EQ(output[5], std::byte{0});
}

TEST(CameraFrame, ConvertsRgb565AndYuv420ToRgba) {
  const std::array<std::byte, 2> red565{std::byte{0}, std::byte{248}};
  std::array<std::byte, 4> rgba{};
  auto rgb = Source(PixelFormat::kRgb565, 1, 1, 2, red565);
  auto destination = Destination(PixelFormat::kRgba8888, 1, 1, 4, rgba);
  ASSERT_TRUE(TransformFrame(rgb, destination, ResampleFilter::kNearest).ok());
  EXPECT_EQ(rgba[0], std::byte{255});
  EXPECT_EQ(rgba[1], std::byte{0});
  EXPECT_EQ(rgba[2], std::byte{0});
  EXPECT_EQ(rgba[3], std::byte{255});

  const std::array<std::byte, 6> yuv{std::byte{16},  std::byte{16},
                                     std::byte{16},  std::byte{16},
                                     std::byte{128}, std::byte{128}};
  FrameView planar;
  planar.layout = {.width = 2,
                   .height = 2,
                   .format = PixelFormat::kYuv420Planar,
                   .stride_bytes = {2, 1, 1},
                   .yuv = YuvEncoding::kBt601Limited};
  planar.planes[0] = std::span<const std::byte>(yuv).first(4);
  planar.planes[1] = std::span<const std::byte>(yuv).subspan(4, 1);
  planar.planes[2] = std::span<const std::byte>(yuv).subspan(5, 1);
  std::array<std::byte, 16> converted{};
  auto output = Destination(PixelFormat::kRgba8888, 2, 2, 8, converted);
  ASSERT_TRUE(TransformFrame(planar, output, ResampleFilter::kBilinear).ok());
  EXPECT_EQ(converted[0], std::byte{0});
  EXPECT_EQ(converted[1], std::byte{0});
  EXPECT_EQ(converted[2], std::byte{0});
  EXPECT_EQ(converted[3], std::byte{255});
  planar.layout.yuv = YuvEncoding::kUnspecified;
  EXPECT_EQ(TransformFrame(planar, output, ResampleFilter::kNearest).code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(CameraFrame, ConvertsRgbaToRgb565WhileScaling) {
  const std::array<std::byte, 8> source{
      std::byte{255}, std::byte{0}, std::byte{0},   std::byte{255},
      std::byte{0},   std::byte{0}, std::byte{255}, std::byte{255}};
  std::array<std::byte, 6> output{};
  const auto input = Source(PixelFormat::kRgba8888, 2, 1, 8, source);
  auto destination = Destination(PixelFormat::kRgb565, 3, 1, 6, output);
  ASSERT_TRUE(
      TransformFrame(input, destination, ResampleFilter::kBilinear).ok());
  EXPECT_EQ(output[0], std::byte{0x00});
  EXPECT_EQ(output[1], std::byte{0xf8});
  EXPECT_EQ(output[2], std::byte{0x10});
  EXPECT_EQ(output[3], std::byte{0x80});
  EXPECT_EQ(output[4], std::byte{0x1f});
  EXPECT_EQ(output[5], std::byte{0x00});
}

TEST(CameraFrame, ConvertsEcamPackedFormatsWithoutSwappingColors) {
  const std::array<std::byte, 2> red_bgr565{std::byte{31}, std::byte{0}};
  const auto packed = Source(PixelFormat::kBgr565, 1, 1, 2, red_bgr565);
  std::array<std::byte, 4> rgba{};
  auto rgba_output = Destination(PixelFormat::kRgba8888, 1, 1, 4, rgba);
  ASSERT_TRUE(
      TransformFrame(packed, rgba_output, ResampleFilter::kNearest).ok());
  EXPECT_EQ(rgba, (std::array<std::byte, 4>{std::byte{255}, std::byte{0},
                                            std::byte{0}, std::byte{255}}));
  std::array<std::byte, 2> rgb565{};
  auto display_output = Destination(PixelFormat::kRgb565, 1, 1, 2, rgb565);
  ASSERT_TRUE(
      TransformFrame(packed, display_output, ResampleFilter::kNearest).ok());
  EXPECT_EQ(rgb565, (std::array<std::byte, 2>{std::byte{0}, std::byte{248}}));

  const std::array<std::byte, 4> rgbx{std::byte{0}, std::byte{0},
                                      std::byte{255}, std::byte{0}};
  const auto rgbx_source = Source(PixelFormat::kRgbx8888, 1, 1, 4, rgbx);
  ASSERT_TRUE(
      TransformFrame(rgbx_source, rgba_output, ResampleFilter::kNearest).ok());
  EXPECT_EQ(rgba, (std::array<std::byte, 4>{std::byte{0}, std::byte{0},
                                            std::byte{255}, std::byte{255}}));
}

TEST(CameraFrame, ConvertsMappedBgrxWithStrideToDisplayColor) {
  const std::array<std::byte, 20> mapped{
      std::byte{0},   std::byte{0},   std::byte{255}, std::byte{17},
      std::byte{255}, std::byte{0},   std::byte{0},   std::byte{42},
      std::byte{99},  std::byte{99},  std::byte{99},  std::byte{99},
      std::byte{0},   std::byte{255}, std::byte{0},   std::byte{7},
      std::byte{255}, std::byte{255}, std::byte{255}, std::byte{8}};
  const auto source = Source(PixelFormat::kBgrx8888, 2, 2, 12, mapped);
  std::array<std::byte, 8> pixels{};
  auto output = Destination(PixelFormat::kRgb565, 2, 2, 4, pixels);
  ASSERT_TRUE(TransformFrame(source, output, ResampleFilter::kNearest).ok());
  EXPECT_EQ(pixels,
            (std::array<std::byte, 8>{
                std::byte{0}, std::byte{248}, std::byte{31}, std::byte{0},
                std::byte{224}, std::byte{7}, std::byte{255}, std::byte{255}}));
}

TEST(CameraFrame, ConvertsBt601Yuv420DirectlyToRgb565) {
  const std::array<std::byte, 6> yuv{std::byte{16},  std::byte{16},
                                     std::byte{16},  std::byte{16},
                                     std::byte{128}, std::byte{128}};
  FrameView source;
  source.layout = {.width = 2,
                   .height = 2,
                   .format = PixelFormat::kYuv420Planar,
                   .stride_bytes = {2, 1, 1},
                   .yuv = YuvEncoding::kBt601Limited};
  source.planes[0] = std::span<const std::byte>(yuv).first(4);
  source.planes[1] = std::span<const std::byte>(yuv).subspan(4, 1);
  source.planes[2] = std::span<const std::byte>(yuv).subspan(5, 1);
  std::array<std::byte, 8> rgb565{};
  auto output = Destination(PixelFormat::kRgb565, 2, 2, 4, rgb565);
  ASSERT_TRUE(TransformFrame(source, output, ResampleFilter::kNearest).ok());
  EXPECT_EQ(rgb565, (std::array<std::byte, 8>{}));
}

TEST(CameraFrame, ResamplesTenBitSamplesAtFullPrecision) {
  const std::array<std::byte, 4> samples{std::byte{0}, std::byte{0},
                                         std::byte{255}, std::byte{3}};
  std::array<std::byte, 6> output{};
  auto source = Source(PixelFormat::kGray16, 2, 1, 4, samples);
  source.layout.bit_depth = 10;
  auto destination = Destination(PixelFormat::kGray16, 3, 1, 6, output);
  destination.layout.bit_depth = 10;
  ASSERT_TRUE(
      TransformFrame(source, destination, ResampleFilter::kBilinear).ok());
  EXPECT_EQ(output[2], std::byte{0});
  EXPECT_EQ(output[3], std::byte{2});
  EXPECT_EQ(output[4], std::byte{255});
  EXPECT_EQ(output[5], std::byte{3});
  std::array<std::byte, 12> preview{};
  auto rgba = Destination(PixelFormat::kRgba8888, 3, 1, 12, preview);
  ASSERT_TRUE(TransformFrame(source, rgba, ResampleFilter::kBilinear).ok());
  EXPECT_EQ(preview[4], std::byte{128});
  EXPECT_EQ(preview[8], std::byte{255});
  destination.layout.bit_depth = 12;
  EXPECT_EQ(
      TransformFrame(source, destination, ResampleFilter::kNearest).code(),
      absl::StatusCode::kInvalidArgument);
}

TEST(CameraFrame, ConvertsTwelveBitRgbPreviewWithoutByteSwapping) {
  const std::array<std::byte, 6> sample{std::byte{255}, std::byte{15},
                                        std::byte{0},   std::byte{0},
                                        std::byte{0},   std::byte{8}};
  auto source = Source(PixelFormat::kRgb161616, 1, 1, 6, sample);
  source.layout.bit_depth = 12;
  std::array<std::byte, 4> preview{};
  auto destination = Destination(PixelFormat::kRgba8888, 1, 1, 4, preview);
  ASSERT_TRUE(
      TransformFrame(source, destination, ResampleFilter::kNearest).ok());
  EXPECT_EQ(preview,
            (std::array<std::byte, 4>{std::byte{255}, std::byte{0},
                                      std::byte{128}, std::byte{255}}));
}

TEST(CameraFrame, PreservesRawBayerAndPlanarTenBitFrames) {
  const std::array<std::byte, 8> mosaic{
      std::byte{1}, std::byte{0}, std::byte{2}, std::byte{0},
      std::byte{3}, std::byte{0}, std::byte{4}, std::byte{0}};
  std::array<std::byte, 8> copied{};
  auto source = Source(PixelFormat::kBayer16, 2, 2, 4, mosaic);
  source.layout.bit_depth = 12;
  source.layout.bayer = BayerPattern::kRggb;
  auto destination = Destination(PixelFormat::kBayer16, 2, 2, 4, copied);
  destination.layout.bit_depth = 12;
  destination.layout.bayer = BayerPattern::kRggb;
  ASSERT_TRUE(
      TransformFrame(source, destination, ResampleFilter::kNearest).ok());
  EXPECT_EQ(copied, mosaic);
  destination.layout.bayer = BayerPattern::kBggr;
  EXPECT_EQ(
      TransformFrame(source, destination, ResampleFilter::kNearest).code(),
      absl::StatusCode::kInvalidArgument);

  const std::array<std::byte, 12> yuv{};
  FrameView planar;
  planar.layout = {.width = 2,
                   .height = 2,
                   .format = PixelFormat::kYuv420Planar16,
                   .stride_bytes = {4, 2, 2},
                   .yuv = YuvEncoding::kBt601Limited,
                   .bit_depth = 10};
  planar.planes[0] = std::span<const std::byte>(yuv).first(8);
  planar.planes[1] = std::span<const std::byte>(yuv).subspan(8, 2);
  planar.planes[2] = std::span<const std::byte>(yuv).subspan(10, 2);
  std::array<std::byte, 12> result{};
  MutableFrameView planar_output;
  planar_output.layout = planar.layout;
  planar_output.planes[0] = std::span<std::byte>(result).first(8);
  planar_output.planes[1] = std::span<std::byte>(result).subspan(8, 2);
  planar_output.planes[2] = std::span<std::byte>(result).subspan(10, 2);
  EXPECT_TRUE(
      TransformFrame(planar, planar_output, ResampleFilter::kNearest).ok());
}

TEST(CameraFrame, RejectsShortAndOverlappingPlanes) {
  std::array<std::byte, 16> storage{};
  auto input = Source(PixelFormat::kRgba8888, 2, 2, 8, storage);
  auto output = Destination(PixelFormat::kRgba8888, 2, 2, 8, storage);
  EXPECT_EQ(TransformFrame(input, output, ResampleFilter::kNearest).code(),
            absl::StatusCode::kInvalidArgument);
  output.planes[0] = storage;
  input.planes[0] = std::span<const std::byte>(storage).first(8);
  EXPECT_EQ(TransformFrame(input, output, ResampleFilter::kNearest).code(),
            absl::StatusCode::kInvalidArgument);
}

class RecordingBackend final {
 public:
  absl::Status Transform(const FrameView& source,
                         const MutableFrameView& destination, ResampleFilter) {
    called = source.handle == 7 && destination.handle == 8;
    return absl::OkStatus();
  }

  FrameBackend Borrow() {
    return {.transform = [this](const FrameView& source,
                                const MutableFrameView& destination,
                                ResampleFilter filter) {
      return Transform(source, destination, filter);
    }};
  }

  bool called = false;
};

TEST(CameraFrame, DispatchesOpaqueFramesWithoutCpuMapping) {
  FrameView source;
  source.layout = {.width = 2, .height = 2, .format = PixelFormat::kRgba8888};
  source.memory = MemoryKind::kGles2Texture;
  source.handle = 7;
  source.device = 1;
  MutableFrameView destination;
  destination.layout = source.layout;
  destination.memory = MemoryKind::kGles2Texture;
  destination.handle = 8;
  destination.device = 1;
  RecordingBackend backend;
  FrameBackend borrowed = backend.Borrow();
  EXPECT_TRUE(
      TransformFrame(source, destination, ResampleFilter::kBilinear, &borrowed)
          .ok());
  EXPECT_TRUE(backend.called);
}

TEST(CameraFrame, LeaseMoveReleasesExactlyOnce) {
  int releases = 0;
  {
    FrameLease original({}, [&releases] { ++releases; });
    FrameLease moved(std::move(original));
    EXPECT_EQ(releases, 0);
  }
  EXPECT_EQ(releases, 1);
}

TEST(CameraFrame, FreshnessUsesCaptureTimeAndDeadline) {
  EXPECT_TRUE(FrameIsFresh({1, 100, ClockDomain::kMonotonic}, 150, 50));
  EXPECT_FALSE(FrameIsFresh({1, 100, ClockDomain::kMonotonic}, 151, 50));
  EXPECT_FALSE(FrameIsFresh({1, 200, ClockDomain::kMonotonic}, 150, 50));
  EXPECT_FALSE(FrameIsFresh({1, 100}, 150, 50));
}

}  // namespace
}  // namespace symbian::api::camera
