// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "symbian/api/camera/frame.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <utility>

namespace symbian::api::camera {
namespace {

constexpr int kMaximumDimension = 16384;

bool IsMemory(MemoryKind memory) {
  return memory == MemoryKind::kLinear || memory == MemoryKind::kMapped;
}

int PlaneCount(PixelFormat format) {
  return format == PixelFormat::kYuv420Planar ||
                 format == PixelFormat::kYuv420Planar16
             ? 3
             : 1;
}

bool IsHighDepth(PixelFormat format) {
  return format == PixelFormat::kGray16 || format == PixelFormat::kRgb161616 ||
         format == PixelFormat::kYuv420Planar16 ||
         format == PixelFormat::kBayer16;
}

int PlaneWidth(const FrameLayout& layout, int plane) {
  return plane == 0 ? layout.width : layout.width / 2;
}

int PlaneHeight(const FrameLayout& layout, int plane) {
  return plane == 0 ? layout.height : layout.height / 2;
}

int PixelBytes(PixelFormat format) {
  if (format == PixelFormat::kRgb161616) {
    return 6;
  }
  if (format == PixelFormat::kGray16 ||
      format == PixelFormat::kYuv420Planar16 ||
      format == PixelFormat::kBayer16) {
    return 2;
  }
  if (format == PixelFormat::kRgb565 || format == PixelFormat::kBgr565) {
    return 2;
  }
  return format == PixelFormat::kRgba8888 || format == PixelFormat::kRgbx8888 ||
                 format == PixelFormat::kBgrx8888
             ? 4
             : 1;
}

absl::Status ValidateLayout(const FrameLayout& layout, MemoryKind memory) {
  switch (layout.format) {
    case PixelFormat::kGray8:
    case PixelFormat::kRgb565:
    case PixelFormat::kRgba8888:
    case PixelFormat::kYuv420Planar:
    case PixelFormat::kBgr565:
    case PixelFormat::kRgbx8888:
    case PixelFormat::kBgrx8888:
    case PixelFormat::kGray16:
    case PixelFormat::kRgb161616:
    case PixelFormat::kYuv420Planar16:
    case PixelFormat::kBayer16:
      break;
    default:
      return absl::InvalidArgumentError("Unknown frame format");
  }
  if (layout.width <= 0 || layout.height <= 0 ||
      layout.width > kMaximumDimension || layout.height > kMaximumDimension) {
    return absl::InvalidArgumentError("Invalid frame dimensions");
  }
  if (IsHighDepth(layout.format)
          ? (layout.bit_depth < 10 || layout.bit_depth > 16)
          : layout.bit_depth != 0) {
    return absl::InvalidArgumentError("Invalid frame sample depth");
  }
  if (layout.format == PixelFormat::kBayer16
          ? layout.bayer == BayerPattern::kUnspecified
          : layout.bayer != BayerPattern::kUnspecified) {
    return absl::InvalidArgumentError("Invalid Bayer pattern");
  }
  if ((layout.format == PixelFormat::kYuv420Planar ||
       layout.format == PixelFormat::kYuv420Planar16) &&
      ((layout.width & 1) != 0 || (layout.height & 1) != 0)) {
    return absl::InvalidArgumentError("YUV420 dimensions must be even");
  }
  for (int plane = 0; plane < PlaneCount(layout.format); ++plane) {
    const int row_bytes = PlaneWidth(layout, plane) *
                          (plane == 0 ? PixelBytes(layout.format)
                                      : (IsHighDepth(layout.format) ? 2 : 1));
    if (IsMemory(memory) && layout.stride_bytes[plane] < row_bytes) {
      return absl::InvalidArgumentError("Frame stride is too small");
    }
  }
  return absl::OkStatus();
}

template <typename Span>
absl::Status ValidateStorage(const FrameLayout& layout, MemoryKind memory,
                             const std::array<Span, 3>& planes,
                             std::uintptr_t handle, std::uintptr_t device) {
  if (!IsMemory(memory)) {
    if (handle == 0 || device == 0) {
      return absl::InvalidArgumentError(
          "Opaque frame needs a handle and device");
    }
    for (const Span plane : planes) {
      if (!plane.empty()) {
        return absl::InvalidArgumentError("Opaque frame has CPU planes");
      }
    }
    return absl::OkStatus();
  }
  if (handle != 0 || device != 0) {
    return absl::InvalidArgumentError("Memory frame has an opaque handle");
  }
  for (int plane = 0; plane < PlaneCount(layout.format); ++plane) {
    const std::size_t rows =
        static_cast<std::size_t>(PlaneHeight(layout, plane));
    const std::size_t row_bytes =
        static_cast<std::size_t>(PlaneWidth(layout, plane)) *
        (plane == 0 ? PixelBytes(layout.format)
                    : (IsHighDepth(layout.format) ? 2 : 1));
    const std::size_t needed =
        (rows - 1) * static_cast<std::size_t>(layout.stride_bytes[plane]) +
        row_bytes;
    if (planes[plane].size() < needed) {
      return absl::InvalidArgumentError(
          "Frame plane is shorter than its layout");
    }
  }
  for (int plane = PlaneCount(layout.format); plane < 3; ++plane) {
    if (!planes[plane].empty()) {
      return absl::InvalidArgumentError("Unexpected frame plane");
    }
  }
  return absl::OkStatus();
}

struct AxisSample {
  int first;
  int second;
  unsigned fraction;
};

AxisSample Position(int destination, int source_extent, int destination_extent,
                    ResampleFilter filter) {
  if (filter == ResampleFilter::kNearest) {
    const int coordinate =
        static_cast<int>(static_cast<std::uint64_t>(destination) *
                         source_extent / destination_extent);
    return {coordinate, coordinate, 0};
  }
  const std::uint64_t centered =
      (static_cast<std::uint64_t>(destination) * 2 + 1) * source_extent * 128 /
      destination_extent;
  if (centered <= 128) {
    return {0, 0, 0};
  }
  const std::uint64_t coordinate = centered - 128;
  const int first =
      std::min(static_cast<int>(coordinate / 256), source_extent - 1);
  return {first, std::min(first + 1, source_extent - 1),
          static_cast<unsigned>(coordinate & 255)};
}

class AxisStepper {
 public:
  AxisStepper(int source_extent, int destination_extent, ResampleFilter filter)
      : source_extent_(source_extent),
        denominator_(static_cast<std::uint64_t>(destination_extent)),
        filter_(filter) {
    const std::uint64_t initial =
        filter == ResampleFilter::kNearest
            ? 0
            : static_cast<std::uint64_t>(source_extent) * 128;
    const std::uint64_t step = static_cast<std::uint64_t>(source_extent) *
                               (filter == ResampleFilter::kNearest ? 1 : 256);
    whole_ = initial / denominator_;
    remainder_ = initial % denominator_;
    step_whole_ = step / denominator_;
    step_remainder_ = step % denominator_;
  }

  AxisSample Next() {
    AxisSample sample;
    if (filter_ == ResampleFilter::kNearest) {
      const int coordinate = static_cast<int>(whole_);
      sample = {coordinate, coordinate, 0};
    } else if (whole_ <= 128) {
      sample = {0, 0, 0};
    } else {
      const std::uint64_t coordinate = whole_ - 128;
      const int first =
          std::min(static_cast<int>(coordinate / 256), source_extent_ - 1);
      sample = {first, std::min(first + 1, source_extent_ - 1),
                static_cast<unsigned>(coordinate & 255)};
    }
    whole_ += step_whole_;
    remainder_ += step_remainder_;
    if (remainder_ >= denominator_) {
      ++whole_;
      remainder_ -= denominator_;
    }
    return sample;
  }

 private:
  int source_extent_;
  std::uint64_t denominator_;
  ResampleFilter filter_;
  std::uint64_t whole_ = 0;
  std::uint64_t remainder_ = 0;
  std::uint64_t step_whole_ = 0;
  std::uint64_t step_remainder_ = 0;
};

unsigned Blend(unsigned top_left, unsigned top_right, unsigned bottom_left,
               unsigned bottom_right, unsigned x, unsigned y) {
  const unsigned top = top_left * (256 - x) + top_right * x;
  const unsigned bottom = bottom_left * (256 - x) + bottom_right * x;
  return (top * (256 - y) + bottom * y + 32768) >> 16;
}

unsigned ReadRgb565(const std::byte* absl_nonnull pixel) {
  return static_cast<unsigned>(std::to_integer<unsigned char>(pixel[0])) |
         (static_cast<unsigned>(std::to_integer<unsigned char>(pixel[1])) << 8);
}

unsigned ReadWord(const std::byte* absl_nonnull pixel) {
  return static_cast<unsigned>(std::to_integer<unsigned char>(pixel[0])) |
         (static_cast<unsigned>(std::to_integer<unsigned char>(pixel[1])) << 8);
}

void WriteWord(std::byte* absl_nonnull pixel, unsigned value) {
  pixel[0] = static_cast<std::byte>(value & 255);
  pixel[1] = static_cast<std::byte>(value >> 8);
}

void WriteRgb565(std::byte* absl_nonnull pixel, unsigned value) {
  pixel[0] = static_cast<std::byte>(value & 255);
  pixel[1] = static_cast<std::byte>(value >> 8);
}

unsigned RgbChannel(unsigned pixel, int shift, unsigned mask) {
  return (pixel >> shift) & mask;
}

void ResamplePlane(std::span<const std::byte> source, int source_stride,
                   int source_width, int source_height,
                   std::span<std::byte> destination, int destination_stride,
                   int destination_width, int destination_height,
                   int pixel_bytes, ResampleFilter filter, bool high_depth) {
  for (int y = 0; y < destination_height; ++y) {
    const AxisSample sy =
        Position(y, source_height, destination_height, filter);
    AxisStepper horizontal(source_width, destination_width, filter);
    for (int x = 0; x < destination_width; ++x) {
      const AxisSample sx = horizontal.Next();
      const std::byte* absl_nonnull top_left =
          source.data() + sy.first * source_stride + sx.first * pixel_bytes;
      const std::byte* absl_nonnull top_right =
          source.data() + sy.first * source_stride + sx.second * pixel_bytes;
      const std::byte* absl_nonnull bottom_left =
          source.data() + sy.second * source_stride + sx.first * pixel_bytes;
      const std::byte* absl_nonnull bottom_right =
          source.data() + sy.second * source_stride + sx.second * pixel_bytes;
      std::byte* absl_nonnull output =
          destination.data() + y * destination_stride + x * pixel_bytes;
      if (high_depth) {
        for (int channel = 0; channel < pixel_bytes; channel += 2) {
          WriteWord(output + channel, Blend(ReadWord(top_left + channel),
                                            ReadWord(top_right + channel),
                                            ReadWord(bottom_left + channel),
                                            ReadWord(bottom_right + channel),
                                            sx.fraction, sy.fraction));
        }
      } else if (pixel_bytes == 2) {
        const unsigned a = ReadRgb565(top_left);
        const unsigned b = ReadRgb565(top_right);
        const unsigned c = ReadRgb565(bottom_left);
        const unsigned d = ReadRgb565(bottom_right);
        const unsigned red = Blend(RgbChannel(a, 11, 31), RgbChannel(b, 11, 31),
                                   RgbChannel(c, 11, 31), RgbChannel(d, 11, 31),
                                   sx.fraction, sy.fraction);
        const unsigned green = Blend(RgbChannel(a, 5, 63), RgbChannel(b, 5, 63),
                                     RgbChannel(c, 5, 63), RgbChannel(d, 5, 63),
                                     sx.fraction, sy.fraction);
        const unsigned blue = Blend(RgbChannel(a, 0, 31), RgbChannel(b, 0, 31),
                                    RgbChannel(c, 0, 31), RgbChannel(d, 0, 31),
                                    sx.fraction, sy.fraction);
        WriteRgb565(output, (red << 11) | (green << 5) | blue);
      } else {
        for (int channel = 0; channel < pixel_bytes; ++channel) {
          output[channel] = static_cast<std::byte>(
              Blend(std::to_integer<unsigned char>(top_left[channel]),
                    std::to_integer<unsigned char>(top_right[channel]),
                    std::to_integer<unsigned char>(bottom_left[channel]),
                    std::to_integer<unsigned char>(bottom_right[channel]),
                    sx.fraction, sy.fraction));
        }
      }
    }
  }
}

unsigned SampleByte(const FrameView& source, int plane, AxisSample sx,
                    AxisSample sy) {
  const std::byte* absl_nonnull top_left =
      source.planes[plane].data() +
      static_cast<std::size_t>(sy.first) * source.layout.stride_bytes[plane] +
      sx.first;
  const std::byte* absl_nonnull top_right =
      source.planes[plane].data() +
      static_cast<std::size_t>(sy.first) * source.layout.stride_bytes[plane] +
      sx.second;
  const std::byte* absl_nonnull bottom_left =
      source.planes[plane].data() +
      static_cast<std::size_t>(sy.second) * source.layout.stride_bytes[plane] +
      sx.first;
  const std::byte* absl_nonnull bottom_right =
      source.planes[plane].data() +
      static_cast<std::size_t>(sy.second) * source.layout.stride_bytes[plane] +
      sx.second;
  return Blend(std::to_integer<unsigned char>(*top_left),
               std::to_integer<unsigned char>(*top_right),
               std::to_integer<unsigned char>(*bottom_left),
               std::to_integer<unsigned char>(*bottom_right), sx.fraction,
               sy.fraction);
}

unsigned SampleRgb565Channel(const FrameView& source, AxisSample sx,
                             AxisSample sy, int shift, unsigned mask) {
  const std::byte* absl_nonnull top_left =
      source.planes[0].data() +
      static_cast<std::size_t>(sy.first) * source.layout.stride_bytes[0] +
      sx.first * 2;
  const std::byte* absl_nonnull top_right =
      source.planes[0].data() +
      static_cast<std::size_t>(sy.first) * source.layout.stride_bytes[0] +
      sx.second * 2;
  const std::byte* absl_nonnull bottom_left =
      source.planes[0].data() +
      static_cast<std::size_t>(sy.second) * source.layout.stride_bytes[0] +
      sx.first * 2;
  const std::byte* absl_nonnull bottom_right =
      source.planes[0].data() +
      static_cast<std::size_t>(sy.second) * source.layout.stride_bytes[0] +
      sx.second * 2;
  const unsigned channel =
      Blend(RgbChannel(ReadRgb565(top_left), shift, mask),
            RgbChannel(ReadRgb565(top_right), shift, mask),
            RgbChannel(ReadRgb565(bottom_left), shift, mask),
            RgbChannel(ReadRgb565(bottom_right), shift, mask), sx.fraction,
            sy.fraction);
  return mask == 31 ? (channel << 3) | (channel >> 2)
                    : (channel << 2) | (channel >> 4);
}

unsigned SampleRgbaChannel(const FrameView& source, AxisSample sx,
                           AxisSample sy, int channel) {
  const std::byte* absl_nonnull top_left =
      source.planes[0].data() +
      static_cast<std::size_t>(sy.first) * source.layout.stride_bytes[0] +
      sx.first * 4 + channel;
  const std::byte* absl_nonnull top_right =
      source.planes[0].data() +
      static_cast<std::size_t>(sy.first) * source.layout.stride_bytes[0] +
      sx.second * 4 + channel;
  const std::byte* absl_nonnull bottom_left =
      source.planes[0].data() +
      static_cast<std::size_t>(sy.second) * source.layout.stride_bytes[0] +
      sx.first * 4 + channel;
  const std::byte* absl_nonnull bottom_right =
      source.planes[0].data() +
      static_cast<std::size_t>(sy.second) * source.layout.stride_bytes[0] +
      sx.second * 4 + channel;
  return Blend(std::to_integer<unsigned char>(*top_left),
               std::to_integer<unsigned char>(*top_right),
               std::to_integer<unsigned char>(*bottom_left),
               std::to_integer<unsigned char>(*bottom_right), sx.fraction,
               sy.fraction);
}

unsigned SampleWordChannel(const FrameView& source, AxisSample sx,
                           AxisSample sy, int channel) {
  const int pixel_bytes = PixelBytes(source.layout.format);
  const int stride = source.layout.stride_bytes[0];
  const std::byte* absl_nonnull base = source.planes[0].data();
  return Blend(
      ReadWord(base + sy.first * stride + sx.first * pixel_bytes + channel * 2),
      ReadWord(base + sy.first * stride + sx.second * pixel_bytes +
               channel * 2),
      ReadWord(base + sy.second * stride + sx.first * pixel_bytes +
               channel * 2),
      ReadWord(base + sy.second * stride + sx.second * pixel_bytes +
               channel * 2),
      sx.fraction, sy.fraction);
}

unsigned ToByte(unsigned sample, int depth) {
  const unsigned maximum = (1u << depth) - 1;
  return (std::min(sample, maximum) * 255 + maximum / 2) / maximum;
}

unsigned ClampByte(int value) {
  return static_cast<unsigned>(std::clamp(value, 0, 255));
}

struct RgbBytes {
  unsigned red;
  unsigned green;
  unsigned blue;
};

RgbBytes SampleYuv420(const FrameView& source, AxisSample luma_x,
                      AxisSample luma_y, AxisSample chroma_x,
                      AxisSample chroma_y) {
  const int luminance =
      static_cast<int>(SampleByte(source, 0, luma_x, luma_y)) - 16;
  const int u =
      static_cast<int>(SampleByte(source, 1, chroma_x, chroma_y)) - 128;
  const int v =
      static_cast<int>(SampleByte(source, 2, chroma_x, chroma_y)) - 128;
  if (source.layout.yuv == YuvEncoding::kBt601Full) {
    const int full_y = luminance + 16;
    return {ClampByte((256 * full_y + 359 * v + 128) >> 8),
            ClampByte((256 * full_y - 88 * u - 183 * v + 128) >> 8),
            ClampByte((256 * full_y + 454 * u + 128) >> 8)};
  }
  const int scaled = 298 * std::max(luminance, 0);
  const bool bt709 = source.layout.yuv == YuvEncoding::kBt709Limited;
  return {ClampByte((scaled + (bt709 ? 459 : 409) * v + 128) >> 8),
          ClampByte((scaled - (bt709 ? 55 : 100) * u - (bt709 ? 136 : 208) * v +
                     128) >>
                    8),
          ClampByte((scaled + (bt709 ? 541 : 516) * u + 128) >> 8)};
}

struct OrientedSample {
  AxisSample x;
  AxisSample y;
};

OrientedSample SamplePosition(int x, int y, int source_width, int source_height,
                              int destination_width, int destination_height,
                              ResampleFilter filter, FrameRotation rotation) {
  const bool quarter_turn = rotation == FrameRotation::kClockwise90 ||
                            rotation == FrameRotation::kClockwise270;
  const int horizontal_source = quarter_turn ? source_height : source_width;
  const int vertical_source = quarter_turn ? source_width : source_height;
  const bool reverse_horizontal = rotation == FrameRotation::kClockwise90 ||
                                  rotation == FrameRotation::k180;
  const bool reverse_vertical = rotation == FrameRotation::kClockwise270 ||
                                rotation == FrameRotation::k180;
  const AxisSample horizontal =
      Position(reverse_horizontal ? destination_width - 1 - x : x,
               horizontal_source, destination_width, filter);
  const AxisSample vertical =
      Position(reverse_vertical ? destination_height - 1 - y : y,
               vertical_source, destination_height, filter);
  return quarter_turn ? OrientedSample{vertical, horizontal}
                      : OrientedSample{horizontal, vertical};
}

void ConvertToRgb565(const FrameView& source,
                     const MutableFrameView& destination, ResampleFilter filter,
                     FrameRotation rotation) {
  const PixelFormat format = source.layout.format;
  if (filter == ResampleFilter::kNearest &&
      (format == PixelFormat::kBgrx8888 || format == PixelFormat::kRgbx8888 ||
       format == PixelFormat::kRgba8888 || format == PixelFormat::kBgr565 ||
       format == PixelFormat::kRgb565)) {
    for (int y = 0; y < destination.layout.height; ++y) {
      std::byte* absl_nonnull output =
          destination.planes[0].data() +
          static_cast<std::size_t>(y) * destination.layout.stride_bytes[0];
      const bool quarter_turn = rotation == FrameRotation::kClockwise90 ||
                                rotation == FrameRotation::kClockwise270;
      const bool reverse_horizontal = rotation == FrameRotation::kClockwise90 ||
                                      rotation == FrameRotation::k180;
      const bool reverse_vertical = rotation == FrameRotation::kClockwise270 ||
                                    rotation == FrameRotation::k180;
      const int vertical_extent =
          quarter_turn ? source.layout.width : source.layout.height;
      const int source_vertical =
          Position(reverse_vertical ? destination.layout.height - 1 - y : y,
                   vertical_extent, destination.layout.height, filter)
              .first;
      AxisStepper horizontal(
          quarter_turn ? source.layout.height : source.layout.width,
          destination.layout.width, filter);
      for (int step = 0; step < destination.layout.width; ++step) {
        const int x =
            reverse_horizontal ? destination.layout.width - 1 - step : step;
        const int source_horizontal = horizontal.Next().first;
        const int source_x = quarter_turn ? source_vertical : source_horizontal;
        const int source_y = quarter_turn ? source_horizontal : source_vertical;
        const std::byte* absl_nonnull pixel =
            source.planes[0].data() +
            static_cast<std::size_t>(source_y) * source.layout.stride_bytes[0] +
            source_x * (format == PixelFormat::kBgr565 ||
                                format == PixelFormat::kRgb565
                            ? 2
                            : 4);
        if (format == PixelFormat::kRgb565) {
          WriteRgb565(output + x * 2, ReadRgb565(pixel));
          continue;
        }
        if (format == PixelFormat::kBgr565) {
          const unsigned packed = ReadRgb565(pixel);
          WriteRgb565(output + x * 2, ((packed & 31) << 11) | (packed & 0x7e0) |
                                          ((packed >> 11) & 31));
          continue;
        }
        const bool blue_first = format == PixelFormat::kBgrx8888;
        const unsigned red =
            std::to_integer<unsigned char>(pixel[blue_first ? 2 : 0]);
        const unsigned green = std::to_integer<unsigned char>(pixel[1]);
        const unsigned blue =
            std::to_integer<unsigned char>(pixel[blue_first ? 0 : 2]);
        WriteRgb565(output + x * 2,
                    ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3));
      }
    }
    return;
  }
  for (int y = 0; y < destination.layout.height; ++y) {
    for (int x = 0; x < destination.layout.width; ++x) {
      const OrientedSample sample =
          SamplePosition(x, y, source.layout.width, source.layout.height,
                         destination.layout.width, destination.layout.height,
                         filter, rotation);
      const AxisSample sx = sample.x;
      const AxisSample sy = sample.y;
      unsigned red;
      unsigned green;
      unsigned blue;
      if (source.layout.format == PixelFormat::kGray8) {
        red = green = blue = SampleByte(source, 0, sx, sy);
      } else if (source.layout.format == PixelFormat::kGray16) {
        red = green = blue = ToByte(SampleWordChannel(source, sx, sy, 0),
                                    source.layout.bit_depth);
      } else if (source.layout.format == PixelFormat::kRgb161616) {
        red = ToByte(SampleWordChannel(source, sx, sy, 0),
                     source.layout.bit_depth);
        green = ToByte(SampleWordChannel(source, sx, sy, 1),
                       source.layout.bit_depth);
        blue = ToByte(SampleWordChannel(source, sx, sy, 2),
                      source.layout.bit_depth);
      } else if (source.layout.format == PixelFormat::kBgr565) {
        red = SampleRgb565Channel(source, sx, sy, 0, 31);
        green = SampleRgb565Channel(source, sx, sy, 5, 63);
        blue = SampleRgb565Channel(source, sx, sy, 11, 31);
      } else if (source.layout.format == PixelFormat::kBgrx8888) {
        red = SampleRgbaChannel(source, sx, sy, 2);
        green = SampleRgbaChannel(source, sx, sy, 1);
        blue = SampleRgbaChannel(source, sx, sy, 0);
      } else if (source.layout.format == PixelFormat::kYuv420Planar) {
        const OrientedSample chroma =
            SamplePosition(x, y, source.layout.width / 2,
                           source.layout.height / 2, destination.layout.width,
                           destination.layout.height, filter, rotation);
        const RgbBytes color = SampleYuv420(source, sx, sy, chroma.x, chroma.y);
        red = color.red;
        green = color.green;
        blue = color.blue;
      } else {
        red = SampleRgbaChannel(source, sx, sy, 0);
        green = SampleRgbaChannel(source, sx, sy, 1);
        blue = SampleRgbaChannel(source, sx, sy, 2);
      }
      std::byte* absl_nonnull output =
          destination.planes[0].data() +
          static_cast<std::size_t>(y) * destination.layout.stride_bytes[0] +
          x * 2;
      WriteRgb565(output,
                  ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3));
    }
  }
}

void ConvertToRgba(const FrameView& source, const MutableFrameView& destination,
                   ResampleFilter filter) {
  for (int y = 0; y < destination.layout.height; ++y) {
    const AxisSample sy =
        Position(y, source.layout.height, destination.layout.height, filter);
    const AxisSample chroma_y =
        source.layout.format == PixelFormat::kYuv420Planar
            ? Position(y, source.layout.height / 2, destination.layout.height,
                       filter)
            : AxisSample{0, 0, 0};
    AxisStepper horizontal(source.layout.width, destination.layout.width,
                           filter);
    AxisStepper chroma_horizontal(source.layout.width / 2,
                                  destination.layout.width, filter);
    for (int x = 0; x < destination.layout.width; ++x) {
      const AxisSample sx = horizontal.Next();
      std::byte* absl_nonnull output =
          destination.planes[0].data() +
          static_cast<std::size_t>(y) * destination.layout.stride_bytes[0] +
          x * 4;
      unsigned red = 0;
      unsigned green = 0;
      unsigned blue = 0;
      if (source.layout.format == PixelFormat::kGray8) {
        red = green = blue = SampleByte(source, 0, sx, sy);
      } else if (source.layout.format == PixelFormat::kGray16) {
        red = green = blue = ToByte(SampleWordChannel(source, sx, sy, 0),
                                    source.layout.bit_depth);
      } else if (source.layout.format == PixelFormat::kRgb161616) {
        red = ToByte(SampleWordChannel(source, sx, sy, 0),
                     source.layout.bit_depth);
        green = ToByte(SampleWordChannel(source, sx, sy, 1),
                       source.layout.bit_depth);
        blue = ToByte(SampleWordChannel(source, sx, sy, 2),
                      source.layout.bit_depth);
      } else if (source.layout.format == PixelFormat::kRgb565) {
        red = SampleRgb565Channel(source, sx, sy, 11, 31);
        green = SampleRgb565Channel(source, sx, sy, 5, 63);
        blue = SampleRgb565Channel(source, sx, sy, 0, 31);
      } else if (source.layout.format == PixelFormat::kBgr565) {
        red = SampleRgb565Channel(source, sx, sy, 0, 31);
        green = SampleRgb565Channel(source, sx, sy, 5, 63);
        blue = SampleRgb565Channel(source, sx, sy, 11, 31);
      } else if (source.layout.format == PixelFormat::kRgbx8888) {
        red = SampleRgbaChannel(source, sx, sy, 0);
        green = SampleRgbaChannel(source, sx, sy, 1);
        blue = SampleRgbaChannel(source, sx, sy, 2);
      } else if (source.layout.format == PixelFormat::kBgrx8888) {
        red = SampleRgbaChannel(source, sx, sy, 2);
        green = SampleRgbaChannel(source, sx, sy, 1);
        blue = SampleRgbaChannel(source, sx, sy, 0);
      } else {
        const AxisSample chroma_x = chroma_horizontal.Next();
        const RgbBytes color = SampleYuv420(source, sx, sy, chroma_x, chroma_y);
        red = color.red;
        green = color.green;
        blue = color.blue;
      }
      output[0] = static_cast<std::byte>(red);
      output[1] = static_cast<std::byte>(green);
      output[2] = static_cast<std::byte>(blue);
      output[3] = std::byte{255};
    }
  }
}

}  // namespace

FrameLease::FrameLease(FrameView view, Release release)
    : view_(std::move(view)), release_(std::move(release)) {}

FrameLease::FrameLease(FrameLease&& other) noexcept
    : view_(std::move(other.view_)),
      release_(std::move(other.release_)) {
  other.release_ = nullptr;
}

FrameLease& FrameLease::operator=(FrameLease&& other) noexcept {
  if (this != &other) {
    Reset();
    view_ = std::move(other.view_);
    release_ = std::move(other.release_);
    other.release_ = nullptr;
  }
  return *this;
}

FrameLease::~FrameLease() {
  Reset();
}

void FrameLease::Reset() {
  if (release_ != nullptr) {
    release_();
    release_ = nullptr;
  }
  view_ = {};
}

FrameRect DetectSolidSideMargins(const FrameView& source) {
  const int width = source.layout.width;
  const int height = source.layout.height;
  const FrameRect complete{0, 0, width, height};
  if (!IsMemory(source.memory) || width < 48 || height < 16) {
    return complete;
  }
  const PixelFormat format = source.layout.format;
  if (format != PixelFormat::kBgrx8888 &&
      format != PixelFormat::kRgbx8888 &&
      format != PixelFormat::kRgba8888 && format != PixelFormat::kRgb565 &&
      format != PixelFormat::kBgr565 && format != PixelFormat::kGray8) {
    return complete;
  }
  const int pixel_bytes = PixelBytes(format);
  const int stride = source.layout.stride_bytes[0];
  if (stride < width * pixel_bytes ||
      source.planes[0].size() <
          static_cast<std::size_t>(height - 1) * stride +
              static_cast<std::size_t>(width) * pixel_bytes) {
    return complete;
  }
  const auto column_has_content = [&](int x) {
    int bright_samples = 0;
    for (int sample = 1; sample <= 8; ++sample) {
      const int y = height * sample / 9;
      const std::byte* absl_nonnull pixel = source.planes[0].data() +
                               static_cast<std::size_t>(y) * stride +
                               static_cast<std::size_t>(x) * pixel_bytes;
      const unsigned int first = std::to_integer<unsigned int>(pixel[0]);
      bool bright = first > 16;
      if (pixel_bytes == 4) {
        bright |= std::to_integer<unsigned int>(pixel[1]) > 16 ||
                  std::to_integer<unsigned int>(pixel[2]) > 16;
      } else if (pixel_bytes == 2) {
        const unsigned int packed =
            first | (std::to_integer<unsigned int>(pixel[1]) << 8);
        bright = (packed & 0xf7de) != 0;
      }
      bright_samples += bright;
      if (bright_samples >= 2) {
        return true;
      }
    }
    return false;
  };
  int left = 0;
  while (left < width && !column_has_content(left)) {
    ++left;
  }
  int right = width;
  while (right > left && !column_has_content(right - 1)) {
    --right;
  }
  const int left_margin = left;
  const int right_margin = width - right;
  if (left_margin < width / 12 || right_margin < width / 12 ||
      std::abs(left_margin - right_margin) > width / 16) {
    return complete;
  }
  const int margin = std::min(left_margin, right_margin);
  if (width - 2 * margin < width / 3) {
    return complete;
  }
  return FrameRect{margin, 0, width - 2 * margin, height};
}

absl::StatusOr<FrameRect> CenteredAspectCrop(int source_width,
                                             int source_height,
                                             int destination_width,
                                             int destination_height) {
  if (source_width <= 0 || source_height <= 0 || destination_width <= 0 ||
      destination_height <= 0) {
    return absl::InvalidArgumentError("Invalid aspect crop dimensions");
  }
  int width = source_width;
  int height = source_height;
  if (static_cast<std::int64_t>(source_width) * destination_height >
      static_cast<std::int64_t>(source_height) * destination_width) {
    width = static_cast<int>(static_cast<std::int64_t>(source_height) *
                             destination_width / destination_height);
  } else {
    height = static_cast<int>(static_cast<std::int64_t>(source_width) *
                              destination_height / destination_width);
  }
  if (width == 0 || height == 0) {
    return absl::InvalidArgumentError("Aspect crop is smaller than one pixel");
  }
  return FrameRect{(source_width - width) / 2, (source_height - height) / 2,
                   width, height};
}

absl::StatusOr<FrameRect> CenteredAspectFit(int source_width,
                                            int source_height,
                                            int destination_width,
                                            int destination_height) {
  if (source_width <= 0 || source_height <= 0 || destination_width <= 0 ||
      destination_height <= 0) {
    return absl::InvalidArgumentError("Invalid aspect fit dimensions");
  }
  int width = destination_width;
  int height = destination_height;
  if (static_cast<std::int64_t>(source_width) * destination_height >
      static_cast<std::int64_t>(source_height) * destination_width) {
    height = static_cast<int>(static_cast<std::int64_t>(destination_width) *
                              source_height / source_width);
  } else {
    width = static_cast<int>(static_cast<std::int64_t>(destination_height) *
                             source_width / source_height);
  }
  if (width <= 0 || height <= 0) {
    return absl::InvalidArgumentError("Aspect fit is smaller than one pixel");
  }
  return FrameRect{(destination_width - width) / 2,
                   (destination_height - height) / 2, width, height};
}

absl::StatusOr<FrameView> CropFrameView(const FrameView& source,
                                        FrameRect crop) {
  absl::Status valid = ValidateLayout(source.layout, source.memory);
  if (!valid.ok()) {
    return valid;
  }
  valid = ValidateStorage(source.layout, source.memory, source.planes,
                          source.handle, source.device);
  if (!valid.ok()) {
    return valid;
  }
  if (!IsMemory(source.memory)) {
    return absl::UnimplementedError("Opaque frame cropping needs a backend");
  }
  if (crop.x < 0 || crop.y < 0 || crop.width <= 0 || crop.height <= 0 ||
      crop.width > source.layout.width - crop.x ||
      crop.height > source.layout.height - crop.y) {
    return absl::InvalidArgumentError("Crop exceeds frame bounds");
  }
  if (PlaneCount(source.layout.format) == 3 &&
      ((crop.x | crop.y | crop.width | crop.height) & 1) != 0) {
    return absl::InvalidArgumentError("YUV420 crop must be even aligned");
  }
  FrameView result = source;
  result.layout.width = crop.width;
  result.layout.height = crop.height;
  for (int plane = 0; plane < PlaneCount(source.layout.format); ++plane) {
    const int divisor = plane == 0 ? 1 : 2;
    const int bytes = plane == 0 ? PixelBytes(source.layout.format)
                                 : (IsHighDepth(source.layout.format) ? 2 : 1);
    const std::size_t offset =
        static_cast<std::size_t>(crop.y / divisor) *
            source.layout.stride_bytes[plane] +
        static_cast<std::size_t>(crop.x / divisor) * bytes;
    result.planes[plane] = source.planes[plane].subspan(offset);
  }
  return result;
}

absl::Status TransformFrame(const FrameView& source,
                            const MutableFrameView& destination,
                            ResampleFilter filter,
                            FrameBackend* absl_nullable backend,
                            FrameRotation rotation) {
  if (rotation != FrameRotation::k0 &&
      rotation != FrameRotation::kClockwise90 &&
      rotation != FrameRotation::k180 &&
      rotation != FrameRotation::kClockwise270) {
    return absl::InvalidArgumentError("Invalid frame rotation");
  }
  absl::Status valid = ValidateLayout(source.layout, source.memory);
  if (!valid.ok()) {
    return valid;
  }
  valid = ValidateLayout(destination.layout, destination.memory);
  if (!valid.ok()) {
    return valid;
  }
  valid = ValidateStorage(source.layout, source.memory, source.planes,
                          source.handle, source.device);
  if (!valid.ok()) {
    return valid;
  }
  valid = ValidateStorage(destination.layout, destination.memory,
                          destination.planes, destination.handle,
                          destination.device);
  if (!valid.ok()) {
    return valid;
  }
  if (!IsMemory(source.memory) || !IsMemory(destination.memory)) {
    if (rotation != FrameRotation::k0) {
      return absl::UnimplementedError(
          "Rotate an opaque frame in its presentation backend");
    }
    return backend == nullptr || !backend->transform
               ? absl::UnimplementedError("Frame backend is required")
               : backend->transform(source, destination, filter);
  }
  const bool convert_to_rgba =
      destination.layout.format == PixelFormat::kRgba8888 &&
      (source.layout.format == PixelFormat::kGray8 ||
       source.layout.format == PixelFormat::kGray16 ||
       source.layout.format == PixelFormat::kRgb161616 ||
       source.layout.format == PixelFormat::kRgb565 ||
       source.layout.format == PixelFormat::kBgr565 ||
       source.layout.format == PixelFormat::kRgbx8888 ||
       source.layout.format == PixelFormat::kBgrx8888 ||
       source.layout.format == PixelFormat::kYuv420Planar);
  const bool convert_to_rgb565 =
      destination.layout.format == PixelFormat::kRgb565 &&
      (source.layout.format != destination.layout.format ||
       rotation != FrameRotation::k0) &&
      (source.layout.format == PixelFormat::kRgba8888 ||
       source.layout.format == PixelFormat::kRgbx8888 ||
       source.layout.format == PixelFormat::kBgrx8888 ||
       source.layout.format == PixelFormat::kBgr565 ||
       source.layout.format == PixelFormat::kRgb565 ||
       source.layout.format == PixelFormat::kYuv420Planar ||
       source.layout.format == PixelFormat::kGray8 ||
       source.layout.format == PixelFormat::kGray16 ||
       source.layout.format == PixelFormat::kRgb161616);
  if (source.layout.format != destination.layout.format && !convert_to_rgba &&
      !convert_to_rgb565) {
    return absl::InvalidArgumentError("Unsupported frame format conversion");
  }
  if (source.layout.format == destination.layout.format &&
      source.layout.bit_depth != destination.layout.bit_depth) {
    return absl::InvalidArgumentError("Frame sample depths differ");
  }
  if (source.layout.format == PixelFormat::kBayer16 &&
      (source.layout.bayer != destination.layout.bayer ||
       source.layout.width != destination.layout.width ||
       source.layout.height != destination.layout.height)) {
    return absl::InvalidArgumentError("Raw mosaic copy needs matching layout");
  }
  if ((source.layout.format == PixelFormat::kYuv420Planar ||
       source.layout.format == PixelFormat::kYuv420Planar16) &&
      (((convert_to_rgba || convert_to_rgb565) &&
        source.layout.yuv == YuvEncoding::kUnspecified) ||
       (!(convert_to_rgba || convert_to_rgb565) &&
        source.layout.yuv != destination.layout.yuv))) {
    return absl::InvalidArgumentError("YUV color encoding is ambiguous");
  }
  for (int source_plane = 0; source_plane < PlaneCount(source.layout.format);
       ++source_plane) {
    const std::span<const std::byte> input = source.planes[source_plane];
    const std::uintptr_t input_begin =
        reinterpret_cast<std::uintptr_t>(input.data());
    for (int destination_plane = 0;
         destination_plane < PlaneCount(destination.layout.format);
         ++destination_plane) {
      const std::span<std::byte> output = destination.planes[destination_plane];
      const std::uintptr_t output_begin =
          reinterpret_cast<std::uintptr_t>(output.data());
      if (input_begin < output_begin + output.size() &&
          output_begin < input_begin + input.size()) {
        return absl::InvalidArgumentError("Overlapping frame planes");
      }
    }
  }
  if (convert_to_rgba) {
    if (rotation != FrameRotation::k0) {
      return absl::UnimplementedError(
          "Rotated conversion to RGBA is unsupported");
    }
    ConvertToRgba(source, destination, filter);
    return absl::OkStatus();
  }
  if (convert_to_rgb565) {
    ConvertToRgb565(source, destination, filter, rotation);
    return absl::OkStatus();
  }
  if (rotation != FrameRotation::k0) {
    return absl::UnimplementedError("Rotated copy requires RGB565 destination");
  }
  for (int plane = 0; plane < PlaneCount(source.layout.format); ++plane) {
    const int width = PlaneWidth(source.layout, plane);
    const int height = PlaneHeight(source.layout, plane);
    const int output_width = PlaneWidth(destination.layout, plane);
    const int output_height = PlaneHeight(destination.layout, plane);
    const int bytes = plane == 0 ? PixelBytes(source.layout.format)
                                 : (IsHighDepth(source.layout.format) ? 2 : 1);
    if (width == output_width && height == output_height) {
      for (int row = 0; row < height; ++row) {
        std::memcpy(destination.planes[plane].data() +
                        row * destination.layout.stride_bytes[plane],
                    source.planes[plane].data() +
                        row * source.layout.stride_bytes[plane],
                    static_cast<std::size_t>(width * bytes));
      }
    } else {
      ResamplePlane(source.planes[plane], source.layout.stride_bytes[plane],
                    width, height, destination.planes[plane],
                    destination.layout.stride_bytes[plane], output_width,
                    output_height, bytes, filter,
                    IsHighDepth(source.layout.format));
    }
  }
  return absl::OkStatus();
}

bool FrameIsFresh(FrameIdentity frame, std::int64_t presentation_time_ns,
                  std::int64_t maximum_age_ns) {
  return frame.clock == ClockDomain::kMonotonic && maximum_age_ns >= 0 &&
         frame.capture_time_ns <= presentation_time_ns &&
         static_cast<std::uint64_t>(presentation_time_ns) -
                 static_cast<std::uint64_t>(frame.capture_time_ns) <=
             static_cast<std::uint64_t>(maximum_age_ns);
}

}  // namespace symbian::api::camera
