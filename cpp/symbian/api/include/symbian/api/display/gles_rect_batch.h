// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_API_DISPLAY_GLES_RECT_BATCH_H_
#define SYMBIAN_API_DISPLAY_GLES_RECT_BATCH_H_

#include <memory>

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace symbian::api::display {

/**
 * @brief Batches ordered solid-color UI rectangles into GLES2 draw calls.
 *
 * Create and Draw require a current GLES2 context on the calling thread. The
 * owner allocates a bounded vertex buffer once; AddRect does not allocate.
 * Flush occurs automatically when the fixed buffer fills. Coordinates are
 * screen pixels with the origin at the top left.
 */
class GlesRectBatch final {
 public:
  static absl::StatusOr<GlesRectBatch> Create();
  static absl::StatusOr<std::unique_ptr<GlesRectBatch>> CreateUnique();
  GlesRectBatch(const GlesRectBatch&) = delete;
  GlesRectBatch& operator=(const GlesRectBatch&) = delete;
  GlesRectBatch(GlesRectBatch&& other) noexcept;
  GlesRectBatch& operator=(GlesRectBatch&& other) noexcept;
  ~GlesRectBatch();

  void Begin(int width, int height);
  void AddRect(int x, int y, int width, int height, float red, float green,
               float blue, float alpha = 1.0f);
  void Draw();
  void Close();

 private:
  GlesRectBatch();
  absl::Status Open();
  struct Impl;
  Impl* absl_nullable impl_ = nullptr;
};

}  // namespace symbian::api::display

#endif  // SYMBIAN_API_DISPLAY_GLES_RECT_BATCH_H_
