// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_API_DISPLAY_GLES2_TEXTURE_H_
#define SYMBIAN_API_DISPLAY_GLES2_TEXTURE_H_

#include <cstdint>
#include <memory>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace symbian::api::display {

// Owns a four-byte-per-pixel GLES2 texture in the current context. Destruction must
// occur while its context is current on the creating thread.
class Gles2Texture final {
 public:
  static absl::StatusOr<Gles2Texture> Create(int width, int height);
  static absl::StatusOr<std::unique_ptr<Gles2Texture>> CreateUnique(int width,
                                                                    int height);
  Gles2Texture(const Gles2Texture&) = delete;
  Gles2Texture& operator=(const Gles2Texture&) = delete;
  Gles2Texture(Gles2Texture&& other) noexcept;
  Gles2Texture& operator=(Gles2Texture&& other) noexcept;
  ~Gles2Texture();

  absl::Status Resize(int width, int height);
  void Close();

  std::uintptr_t handle() const { return texture_; }

  int width() const { return width_; }

  int height() const { return height_; }

 private:
  Gles2Texture() = default;
  unsigned int texture_ = 0;
  int width_ = 0;
  int height_ = 0;
};

struct Gles2Viewport {
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
};

// Scale an image to cover a surface without stretching it. The centered
// viewport may extend beyond the surface; GLES clips the excess pixels.
absl::StatusOr<Gles2Viewport> AspectFillViewport(int surface_width,
                                                 int surface_height,
                                                 int image_width,
                                                 int image_height);

// Preserve the complete image inside a surface, leaving unused bands when
// their aspect ratios differ.
absl::StatusOr<Gles2Viewport> AspectFitViewport(int surface_width,
                                                int surface_height,
                                                int image_width,
                                                int image_height);

// Describes byte order, row origin and a presentation rotation. Rotation is
// applied in the shader without changing the uploaded texture.
enum class TextureRotation { k0, kClockwise90, k180, kClockwise270 };

struct Gles2TextureSampling {
  bool blue_first = false;
  bool top_down = false;
  TextureRotation rotation = TextureRotation::k0;
  // Normalized source bounds with a top-left origin. Full texture by default.
  float source_left = 0.0f;
  float source_top = 0.0f;
  float source_right = 1.0f;
  float source_bottom = 1.0f;
};

// Draws any RGBA8888 texture into the current GLES2 framebuffer. Calls use
// an exclusive context: this helper changes its GL state and does not restore
// it. The caller owns context activation and swap timing.
class Gles2TexturePresenter final {
 public:
  static absl::StatusOr<Gles2TexturePresenter> Create();
  static absl::StatusOr<std::unique_ptr<Gles2TexturePresenter>> CreateUnique();
  Gles2TexturePresenter(const Gles2TexturePresenter&) = delete;
  Gles2TexturePresenter& operator=(const Gles2TexturePresenter&) = delete;
  Gles2TexturePresenter(Gles2TexturePresenter&& other) noexcept;
  Gles2TexturePresenter& operator=(Gles2TexturePresenter&& other) noexcept;
  ~Gles2TexturePresenter();

  absl::Status Clear(int width, int height, float red, float green, float blue);
  absl::Status Draw(const Gles2Texture& texture, Gles2Viewport viewport,
                    Gles2TextureSampling sampling = {});
  void Close();

 private:
  Gles2TexturePresenter() = default;
  absl::Status Open();
  unsigned int program_ = 0;
  int sampler_ = -1;
  int blue_first_ = -1;
  int top_down_ = -1;
  int rotation_ = -1;
  int source_rect_ = -1;
};

}  // namespace symbian::api::display

#endif  // SYMBIAN_API_DISPLAY_GLES2_TEXTURE_H_
