// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_API_CAMERA_CAMERA_STREAM_H_
#define SYMBIAN_API_CAMERA_CAMERA_STREAM_H_

#include <functional>
#include <optional>
#include <span>
#include <utility>

#include <absl/base/nullability.h>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "symbian/api/camera/frame.h"

namespace symbian::api::camera {

// The callable runs during one synchronous poll. The frame view is valid only
// during consume; the callback cannot retain it or close the source.
struct FrameConsumer {
  std::function<absl::Status(const FrameView&)> consume;
};

// Non-owning device-backend view. Captured owners must outlive every call and
// returned frame lease. Backends provide operations without subclassing an SDK
// object.
struct CameraSource {
  std::function<absl::Status(int, std::span<const FrameLayout>)> open;
  std::function<absl::StatusOr<std::optional<FrameLease>>()> poll;
  std::function<absl::StatusOr<bool>(FrameConsumer)> poll_scoped;
  std::function<void()> close;

  absl::Status Open(int index, std::span<const FrameLayout> preferences) const {
    if (!open) {
      return absl::FailedPreconditionError("Camera source has no open operation");
    }
    return open(index, preferences);
  }

  absl::StatusOr<std::optional<FrameLease>> Poll() const {
    if (!poll) {
      return absl::FailedPreconditionError("Camera source has no poll operation");
    }
    return poll();
  }

  absl::StatusOr<bool> PollScoped(FrameConsumer consumer) const {
    if (!poll_scoped) {
      return absl::FailedPreconditionError(
          "Camera source has no scoped poll operation");
    }
    return poll_scoped(std::move(consumer));
  }

  void Close() const {
    if (close) {
      close();
    }
  }
};

// ECam viewfinder requests and callbacks stay on the opening OS thread. Poll
// pumps a bounded number of ready active objects; it never waits for a frame.
// Each returned frame is a movable lease. Its destruction releases the native
// buffer on this same thread; stream destruction defers final native cleanup
// until an outstanding lease is released. Only one frame can be leased at a
// time. Camera reservation, power and frame arrival are asynchronous.
class CameraStream final {
 public:
  CameraStream() = default;
  CameraStream(const CameraStream&) = delete;
  CameraStream& operator=(const CameraStream&) = delete;
  ~CameraStream();

  // Tries preferred formats in order after reservation and power-on. ECam
  // chooses the final dimensions. Poll reports failure if all are rejected.
  absl::Status Open(int index, std::span<const FrameLayout> preferences);
  absl::Status Open(int index, FrameLayout preferred);
  absl::StatusOr<std::optional<FrameLease>> Poll();
  absl::StatusOr<bool> PollScoped(FrameConsumer consumer);
  void Close();
  CameraSource Borrow();

 private:
  void* absl_nullable state_ = nullptr;
};

}  // namespace symbian::api::camera

#endif  // SYMBIAN_API_CAMERA_CAMERA_STREAM_H_
