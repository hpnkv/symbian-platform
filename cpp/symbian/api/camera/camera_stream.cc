// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "symbian/api/camera/camera_stream.h"

#include <array>
#include <cstddef>
#include <cstdint>

#include <absl/status/status_macros.h>

#include "native_camera_stream.h"
#include "symbian/native_status.h"

namespace symbian::api::camera {
namespace {

FrameView NativeFrameView(const NativeCameraFrame& native) {
  FrameView view;
  view.layout = {.width = native.width,
                 .height = native.height,
                 .format = static_cast<PixelFormat>(native.format),
                 .stride_bytes = {native.stride, 0, 0}};
  view.identity = {.sequence = native.sequence,
                   .capture_time_ns = native.capture_time_ns,
                   .clock = ClockDomain::kMonotonic};
  view.memory = native.mapped ? MemoryKind::kMapped : MemoryKind::kLinear;
  const auto* absl_nonnull bytes =
      reinterpret_cast<const std::byte*>(native.data);
  if (view.layout.format == PixelFormat::kYuv420Planar) {
    const std::size_t y_bytes =
        static_cast<std::size_t>(native.width) * native.height;
    const std::size_t chroma_bytes = y_bytes / 4;
    view.layout.stride_bytes = {native.width, native.width / 2,
                                native.width / 2};
    view.layout.yuv = YuvEncoding::kBt601Limited;
    view.planes[0] = {bytes, y_bytes};
    view.planes[1] = {bytes + y_bytes, chroma_bytes};
    view.planes[2] = {bytes + y_bytes + chroma_bytes, chroma_bytes};
  } else {
    view.planes[0] = {bytes, static_cast<std::size_t>(native.bytes)};
  }
  return view;
}

struct ScopedConsume {
  FrameConsumer consumer;
  absl::Status status = absl::OkStatus();
};

void ConsumeNativeFrame(const NativeCameraFrame* absl_nonnull native,
                        void* absl_nonnull context) {
  auto* absl_nonnull scoped = static_cast<ScopedConsume*>(context);
  scoped->status = scoped->consumer.consume(NativeFrameView(*native));
}

}  // namespace

CameraStream::~CameraStream() {
  Close();
}

absl::Status CameraStream::Open(int index,
                                std::span<const FrameLayout> preferences) {
  if (state_ != nullptr) {
    return absl::FailedPreconditionError("Camera stream already open");
  }
  if (index < 0 || preferences.empty() || preferences.size() > 8) {
    return absl::InvalidArgumentError("Unsupported ECam viewfinder request");
  }
  std::array<NativeCameraRequest, 8> native_requests{};
  for (std::size_t i = 0; i < preferences.size(); ++i) {
    const FrameLayout& preferred = preferences[i];
    if (preferred.width <= 0 || preferred.height <= 0 ||
        preferred.width > 8192 || preferred.height > 8192 ||
        (preferred.format == PixelFormat::kYuv420Planar &&
         ((preferred.width & 1) != 0 || (preferred.height & 1) != 0)) ||
        (preferred.format != PixelFormat::kGray8 &&
         preferred.format != PixelFormat::kBgr565 &&
         preferred.format != PixelFormat::kRgbx8888 &&
         preferred.format != PixelFormat::kYuv420Planar)) {
      return absl::InvalidArgumentError("Unsupported ECam viewfinder request");
    }
    native_requests[i] = {.width = preferred.width,
                          .height = preferred.height,
                          .format = static_cast<int>(preferred.format)};
  }
  const int error = SymbianDeviceCameraStreamCreate(
      index, native_requests.data(), static_cast<int>(preferences.size()),
      &state_);
  return symbian::StatusFromNativeError(error, "Open ECam stream");
}

absl::Status CameraStream::Open(int index, FrameLayout preferred) {
  return Open(index, std::span<const FrameLayout>(&preferred, 1));
}

absl::StatusOr<std::optional<FrameLease>> CameraStream::Poll() {
  if (state_ == nullptr) {
    return absl::FailedPreconditionError("Camera stream is closed");
  }
  NativeCameraFrame native;
  if (const int error = SymbianDeviceCameraStreamPoll(state_, &native);
      error != 0) {
    return symbian::StatusFromNativeError(
        error, error == symbian::native_error::kInUse ? "Camera is in use"
                                                      : "Poll ECam stream");
  }
  if (native.data == nullptr) {
    return std::nullopt;
  }
  return std::optional<FrameLease>(std::in_place, NativeFrameView(native),
                                   [owner = native.release_owner] {
                                     SymbianDeviceCameraFrameRelease(owner);
                                   });
}

absl::StatusOr<bool> CameraStream::PollScoped(FrameConsumer consumer) {
  if (state_ == nullptr) {
    return absl::FailedPreconditionError("Camera stream is closed");
  }
  if (!consumer.consume) {
    return absl::InvalidArgumentError("Camera frame consumer is empty");
  }
  ScopedConsume scoped{.consumer = std::move(consumer)};
  bool delivered = false;
  if (const int error = SymbianDeviceCameraStreamPollScoped(
          state_, ConsumeNativeFrame, &scoped, &delivered);
      error != 0) {
    return symbian::StatusFromNativeError(
        error, error == symbian::native_error::kInUse ? "Camera is in use"
                                                      : "Poll ECam stream");
  }
  ABSL_RETURN_IF_ERROR(scoped.status);
  return delivered;
}

CameraSource CameraStream::Borrow() {
  return {.open =
              [this](int index, std::span<const FrameLayout> preferences) {
                return Open(index, preferences);
              },
          .poll = [this] { return Poll(); },
          .poll_scoped =
              [this](FrameConsumer consumer) {
                return PollScoped(std::move(consumer));
              },
          .close = [this] { Close(); }};
}

void CameraStream::Close() {
  SymbianDeviceCameraStreamClose(state_);
  state_ = nullptr;
}

}  // namespace symbian::api::camera
