// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_API_CAMERA_GLES2_FRAME_BACKEND_H_
#define SYMBIAN_API_CAMERA_GLES2_FRAME_BACKEND_H_

#include <cstdint>

#include <absl/base/nullability.h>

#include "absl/status/status.h"
#include "symbian/api/camera/frame.h"

namespace symbian::api::camera {

enum class Gles2ContextUse { kPreserveState, kExclusive };

// GLES2 RGBA8888 transfers and resampling on the caller's current context.
// Textures remain owned by their producer/consumer. A nonzero device token
// identifies the current context; the caller supplies the same token to both
// texture views. Use kExclusive only when this backend alone owns the current
// context's GL state. This skips state queries and restoration; it is
// useful for a dedicated upload/processing context. CPU rows are top-down,
// while GL stores bottom-up, so transfers
// reverse row order. Texture-to-texture operations stay on the GPU.
// Client upload bytes may be released when the call returns. Texture work
// remains ordered in this context; callers using another context supply fences.
// Producers in another context must make their input ready before the call.
class Gles2FrameBackend final {
 public:
  explicit Gles2FrameBackend(
      std::uintptr_t device,
      Gles2ContextUse context_use = Gles2ContextUse::kPreserveState);
  Gles2FrameBackend(const Gles2FrameBackend&) = delete;
  Gles2FrameBackend& operator=(const Gles2FrameBackend&) = delete;
  ~Gles2FrameBackend();

  absl::Status Transform(const FrameView& source,
                         const MutableFrameView& destination,
                         ResampleFilter filter);
  // Non-owning transform view valid only while this backend is alive.
  FrameBackend Borrow();
  void Close();

 private:
  struct Impl;
  Impl* absl_nullable impl_ = nullptr;
  std::uintptr_t device_;
  Gles2ContextUse context_use_;
};

}  // namespace symbian::api::camera

#endif  // SYMBIAN_API_CAMERA_GLES2_FRAME_BACKEND_H_
