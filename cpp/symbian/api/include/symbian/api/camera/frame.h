// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_API_CAMERA_FRAME_H_
#define SYMBIAN_API_CAMERA_FRAME_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>

#include <absl/base/nullability.h>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace symbian::api::camera {

enum class PixelFormat {
  kGray8,
  kRgb565,
  kRgba8888,
  kYuv420Planar,
  // ECam's packed RGB formats place red in the least significant bits.
  kBgr565,
  kRgbx8888,
  // Little-endian 16-bit samples. The significant bit count is in bit_depth;
  // unused high bits are zero. These layouts preserve 10/12/16-bit capture.
  kGray16,
  kRgb161616,
  kYuv420Planar16,
  // One 16-bit sample per sensor photosite. This is a raw mosaic, not RGB.
  kBayer16,
  // Byte order B, G, R, unused on little-endian EColor16MU bitmap memory.
  kBgrx8888,
};

enum class BayerPattern { kUnspecified, kRggb, kGrbg, kGbrg, kBggr };

enum class YuvEncoding {
  kUnspecified,
  kBt601Limited,
  kBt601Full,
  kBt709Limited
};
enum class ClockDomain { kUnknown, kMonotonic };

// Mapped memory can be an ECam RChunk; a texture or native image remains
// opaque. No operation silently maps or downloads an opaque image.
enum class MemoryKind { kLinear, kMapped, kGles2Texture, kNativeImage };

struct FrameLayout {
  int width = 0;
  int height = 0;
  PixelFormat format = PixelFormat::kRgb565;
  std::array<int, 3> stride_bytes{};
  YuvEncoding yuv = YuvEncoding::kUnspecified;
  // Set to 10..16 for 16-bit sample formats. Zero means the format's fixed
  // precision. No implicit tone mapping or transfer function is assumed.
  int bit_depth = 0;
  BayerPattern bayer = BayerPattern::kUnspecified;
};

struct FrameIdentity {
  std::uint64_t sequence = 0;
  std::int64_t capture_time_ns = 0;
  ClockDomain clock = ClockDomain::kUnknown;
};

// The producer owns this storage until its frame lease is released. For GLES2,
// handle is a texture name and device identifies its current GL context;
// native images use an adapter-defined handle/device pair. Opaque images have
// empty planes. The ready/completion ordering is the backend's contract.
struct FrameView {
  FrameLayout layout;
  FrameIdentity identity;
  MemoryKind memory = MemoryKind::kLinear;
  std::array<std::span<const std::byte>, 3> planes{};
  std::uintptr_t handle = 0;
  std::uintptr_t device = 0;
};

struct MutableFrameView {
  FrameLayout layout;
  MemoryKind memory = MemoryKind::kLinear;
  std::array<std::span<std::byte>, 3> planes{};
  std::uintptr_t handle = 0;
  std::uintptr_t device = 0;
};

// A movable frame keeps a native producer buffer borrowed until destruction.
// Release happens on the thread that destroys the lease. A backend that needs
// thread affinity must arrange for that destruction on its owning thread.
class FrameLease final {
 public:
  using Release = std::function<void()>;

  FrameLease() = default;
  FrameLease(FrameView view, Release release);
  FrameLease(const FrameLease&) = delete;
  FrameLease& operator=(const FrameLease&) = delete;
  FrameLease(FrameLease&& other) noexcept;
  FrameLease& operator=(FrameLease&& other) noexcept;
  ~FrameLease();

  const FrameView& view() const { return view_; }

  void Reset();

 private:
  FrameView view_;
  Release release_;
};

enum class ResampleFilter { kNearest, kBilinear };

// Presentation transform for a fixed camera frame. This changes sampling
// coordinates only; no rotated source buffer is allocated.
enum class FrameRotation { k0, kClockwise90, k180, kClockwise270 };

struct FrameRect {
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
};

// Detect symmetric, nearly black side margins in a CPU-accessible packed
// preview frame. Returns the complete frame when the margins are uncertain or
// the format is unsupported. This is a presentation hint, not a sensor crop.
FrameRect DetectSolidSideMargins(const FrameView& source);

// Center-crop to the destination aspect ratio, preserving the source's full
// height for a tall destination or full width for a wide destination.
absl::StatusOr<FrameRect> CenteredAspectCrop(int source_width,
                                             int source_height,
                                             int destination_width,
                                             int destination_height);

// Fits every source pixel into a destination without changing its aspect
// ratio. The returned rectangle is centered in destination coordinates.
absl::StatusOr<FrameRect> CenteredAspectFit(int source_width,
                                            int source_height,
                                            int destination_width,
                                            int destination_height);

// Borrows a rectangular region of a linear or mapped frame without copying.
// Chroma-subsampled frames require even crop boundaries and dimensions.
absl::StatusOr<FrameView> CropFrameView(const FrameView& source,
                                        FrameRect crop);

// Device backends implement direct texture/native transfers, scaling and
// synchronization. They may return Unimplemented for unsupported pairs. The
// caller supplies one when either frame is opaque; linear/mapped frames use
// the common stride-aware implementation without an intermediate allocation.
struct FrameBackend {
  // The captured backend owner must outlive this view and every transform.
  std::function<absl::Status(const FrameView&, const MutableFrameView&,
                             ResampleFilter)> transform;
};

// A nonzero rotation is sampled directly into a linear or mapped RGB565
// destination, without a rotated source allocation. Opaque frames rotate in
// their presentation backend (for GLES2, use Gles2TexturePresenter).
absl::Status TransformFrame(const FrameView& source,
                            const MutableFrameView& destination,
                            ResampleFilter filter,
                            FrameBackend* absl_nullable backend = nullptr,
                            FrameRotation rotation = FrameRotation::k0);

// Decide whether an already available frame is still useful at the next
// presentation deadline. This does not own a timer or block a capture thread.
bool FrameIsFresh(FrameIdentity frame, std::int64_t presentation_time_ns,
                  std::int64_t maximum_age_ns);

}  // namespace symbian::api::camera

#endif  // SYMBIAN_API_CAMERA_FRAME_H_
