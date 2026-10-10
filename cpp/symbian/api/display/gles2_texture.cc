// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "symbian/api/display/gles2_texture.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <utility>

#include <GLES2/gl2.h>
#include <absl/status/status_macros.h>
#include <absl/base/nullability.h>

namespace symbian::api::display {
namespace {

constexpr char kVertex[] =
    "attribute vec2 aPosition; attribute vec2 aTexture; "
    "varying mediump vec2 vTexture; "
    "void main() { vTexture = aTexture; "
    "gl_Position = vec4(aPosition, 0.0, 1.0); }";
constexpr char kFragment[] =
    "precision mediump float; varying mediump vec2 vTexture; "
    "uniform sampler2D uImage; uniform float uBlueFirst; "
    "uniform float uTopDown; uniform float uRotation; "
    "uniform vec4 uSourceRect; "
    "void main() { vec2 p = vTexture; "
    "if (uRotation > 2.5) p = vec2(p.y, 1.0 - p.x); "
    "else if (uRotation > 1.5) p = vec2(1.0 - p.x, 1.0 - p.y); "
    "else if (uRotation > 0.5) p = vec2(1.0 - p.y, p.x); "
    "p = vec2(mix(uSourceRect.x, uSourceRect.z, p.x), "
    "mix(1.0 - uSourceRect.w, 1.0 - uSourceRect.y, p.y)); "
    "vec2 coord = vec2(p.x, mix(p.y, 1.0 - p.y, uTopDown)); "
    "vec4 c = texture2D(uImage, coord); "
    "gl_FragColor = vec4(mix(c.rgb, c.bgr, uBlueFirst), "
    "mix(c.a, 1.0, uBlueFirst)); }";
constexpr GLfloat kQuad[] = {
    -1, -1, 0, 0, 1, -1, 1, 0, -1, 1, 0, 1, 1, 1, 1, 1,
};

GLuint Compile(GLenum kind, const char* absl_nonnull source) {
  const GLuint shader = glCreateShader(kind);
  if (shader == 0) {
    return 0;
  }
  glShaderSource(shader, 1, &source, nullptr);
  glCompileShader(shader);
  GLint compiled = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
  if (compiled == 0) {
    glDeleteShader(shader);
    return 0;
  }
  return shader;
}

absl::Status GlStatus(const char* absl_nonnull operation) {
  return glGetError() == GL_NO_ERROR ? absl::OkStatus()
                                     : absl::UnavailableError(operation);
}

}  // namespace

absl::StatusOr<Gles2Viewport> AspectFillViewport(int surface_width,
                                                 int surface_height,
                                                 int image_width,
                                                 int image_height) {
  if (surface_width <= 0 || surface_height <= 0 || image_width <= 0 ||
      image_height <= 0) {
    return absl::InvalidArgumentError("Invalid aspect fill dimensions");
  }
  std::int64_t width = surface_width;
  std::int64_t height = surface_height;
  if (static_cast<std::int64_t>(surface_width) * image_height <
      static_cast<std::int64_t>(surface_height) * image_width) {
    width = (static_cast<std::int64_t>(surface_height) * image_width +
             image_height - 1) /
            image_height;
  } else {
    height = (static_cast<std::int64_t>(surface_width) * image_height +
              image_width - 1) /
             image_width;
  }
  if (width > std::numeric_limits<int>::max() ||
      height > std::numeric_limits<int>::max()) {
    return absl::InvalidArgumentError("Aspect fill viewport is too large");
  }
  return Gles2Viewport{static_cast<int>((surface_width - width) / 2),
                       static_cast<int>((surface_height - height) / 2),
                       static_cast<int>(width), static_cast<int>(height)};
}

absl::StatusOr<Gles2Viewport> AspectFitViewport(int surface_width,
                                                int surface_height,
                                                int image_width,
                                                int image_height) {
  if (surface_width <= 0 || surface_height <= 0 || image_width <= 0 ||
      image_height <= 0) {
    return absl::InvalidArgumentError("Invalid aspect fit dimensions");
  }
  std::int64_t width = surface_width;
  std::int64_t height = surface_height;
  if (static_cast<std::int64_t>(surface_width) * image_height >
      static_cast<std::int64_t>(surface_height) * image_width) {
    width =
        static_cast<std::int64_t>(surface_height) * image_width / image_height;
  } else {
    height =
        static_cast<std::int64_t>(surface_width) * image_height / image_width;
  }
  if (width == 0 || height == 0) {
    return absl::InvalidArgumentError("Aspect fit viewport is too small");
  }
  return Gles2Viewport{static_cast<int>((surface_width - width) / 2),
                       static_cast<int>((surface_height - height) / 2),
                       static_cast<int>(width), static_cast<int>(height)};
}

Gles2Texture::Gles2Texture(Gles2Texture&& other) noexcept
    : texture_(std::exchange(other.texture_, 0)),
      width_(std::exchange(other.width_, 0)),
      height_(std::exchange(other.height_, 0)) {}

Gles2Texture& Gles2Texture::operator=(Gles2Texture&& other) noexcept {
  if (this != &other) {
    Close();
    texture_ = std::exchange(other.texture_, 0);
    width_ = std::exchange(other.width_, 0);
    height_ = std::exchange(other.height_, 0);
  }
  return *this;
}

absl::StatusOr<Gles2Texture> Gles2Texture::Create(int width, int height) {
  Gles2Texture result;
  ABSL_RETURN_IF_ERROR(result.Resize(width, height));
  return result;
}

absl::StatusOr<std::unique_ptr<Gles2Texture>> Gles2Texture::CreateUnique(
    int width, int height) {
  ABSL_ASSIGN_OR_RETURN(auto created, Create(width, height));
  std::unique_ptr<Gles2Texture> owner(new (std::nothrow)
                                          Gles2Texture(std::move(created)));
  if (owner == nullptr) {
    return absl::ResourceExhaustedError(
        "GLES2 texture owner allocation failed");
  }
  return owner;
}

Gles2Texture::~Gles2Texture() {
  Close();
}

absl::Status Gles2Texture::Resize(int width, int height) {
  if (width <= 0 || height <= 0) {
    return absl::InvalidArgumentError("Invalid GLES2 texture size");
  }
  if (texture_ == 0) {
    glGenTextures(1, &texture_);
    if (texture_ == 0) {
      return absl::UnavailableError("Create GLES2 texture");
    }
  }
  glBindTexture(GL_TEXTURE_2D, texture_);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  if (absl::Status result = GlStatus("Allocate GLES2 texture"); !result.ok()) {
    Close();
    return result;
  }
  width_ = width;
  height_ = height;
  return absl::OkStatus();
}

void Gles2Texture::Close() {
  if (texture_ != 0) {
    glDeleteTextures(1, &texture_);
    texture_ = 0;
  }
  width_ = 0;
  height_ = 0;
}

Gles2TexturePresenter::Gles2TexturePresenter(
    Gles2TexturePresenter&& other) noexcept
    : program_(std::exchange(other.program_, 0)),
      sampler_(std::exchange(other.sampler_, -1)),
      blue_first_(std::exchange(other.blue_first_, -1)),
      top_down_(std::exchange(other.top_down_, -1)),
      rotation_(std::exchange(other.rotation_, -1)),
      source_rect_(std::exchange(other.source_rect_, -1)) {}

Gles2TexturePresenter& Gles2TexturePresenter::operator=(
    Gles2TexturePresenter&& other) noexcept {
  if (this != &other) {
    Close();
    program_ = std::exchange(other.program_, 0);
    sampler_ = std::exchange(other.sampler_, -1);
    blue_first_ = std::exchange(other.blue_first_, -1);
    top_down_ = std::exchange(other.top_down_, -1);
    rotation_ = std::exchange(other.rotation_, -1);
    source_rect_ = std::exchange(other.source_rect_, -1);
  }
  return *this;
}

absl::StatusOr<Gles2TexturePresenter> Gles2TexturePresenter::Create() {
  Gles2TexturePresenter result;
  ABSL_RETURN_IF_ERROR(result.Open());
  return result;
}

absl::StatusOr<std::unique_ptr<Gles2TexturePresenter>>
Gles2TexturePresenter::CreateUnique() {
  ABSL_ASSIGN_OR_RETURN(auto created, Create());
  std::unique_ptr<Gles2TexturePresenter> owner(
      new (std::nothrow) Gles2TexturePresenter(std::move(created)));
  if (owner == nullptr) {
    return absl::ResourceExhaustedError(
        "GLES2 presenter owner allocation failed");
  }
  return owner;
}

Gles2TexturePresenter::~Gles2TexturePresenter() {
  Close();
}

absl::Status Gles2TexturePresenter::Open() {
  if (program_ != 0) {
    return absl::FailedPreconditionError("GLES2 presenter already open");
  }
  const GLuint vertex = Compile(GL_VERTEX_SHADER, kVertex);
  const GLuint fragment = Compile(GL_FRAGMENT_SHADER, kFragment);
  if (vertex == 0 || fragment == 0) {
    if (vertex != 0) {
      glDeleteShader(vertex);
    }
    if (fragment != 0) {
      glDeleteShader(fragment);
    }
    return absl::UnavailableError("Compile GLES2 presenter shaders");
  }
  program_ = glCreateProgram();
  if (program_ != 0) {
    glAttachShader(program_, vertex);
    glAttachShader(program_, fragment);
    glBindAttribLocation(program_, 0, "aPosition");
    glBindAttribLocation(program_, 1, "aTexture");
    glLinkProgram(program_);
  }
  glDeleteShader(vertex);
  glDeleteShader(fragment);
  GLint linked = 0;
  if (program_ != 0) {
    glGetProgramiv(program_, GL_LINK_STATUS, &linked);
  }
  if (linked == 0) {
    Close();
    return absl::UnavailableError("Link GLES2 presenter program");
  }
  sampler_ = glGetUniformLocation(program_, "uImage");
  blue_first_ = glGetUniformLocation(program_, "uBlueFirst");
  top_down_ = glGetUniformLocation(program_, "uTopDown");
  rotation_ = glGetUniformLocation(program_, "uRotation");
  source_rect_ = glGetUniformLocation(program_, "uSourceRect");
  return GlStatus("Open GLES2 presenter");
}

absl::Status Gles2TexturePresenter::Clear(int width, int height, float red,
                                          float green, float blue) {
  if (program_ == 0 || width <= 0 || height <= 0) {
    return absl::FailedPreconditionError("GLES2 presenter is not ready");
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, width, height);
  glDisable(GL_SCISSOR_TEST);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glClearColor(red, green, blue, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  return GlStatus("Clear GLES2 framebuffer");
}

absl::Status Gles2TexturePresenter::Draw(const Gles2Texture& texture,
                                         Gles2Viewport viewport,
                                         Gles2TextureSampling sampling) {
  if (program_ == 0 || texture.handle() == 0 || viewport.width <= 0 ||
      viewport.height <= 0 || sampling.source_left < 0.0f ||
      sampling.source_top < 0.0f || sampling.source_right > 1.0f ||
      sampling.source_bottom > 1.0f ||
      sampling.source_left >= sampling.source_right ||
      sampling.source_top >= sampling.source_bottom) {
    return absl::FailedPreconditionError("GLES2 texture draw is not ready");
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(viewport.x, viewport.y, viewport.width, viewport.height);
  glDisable(GL_BLEND);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glUseProgram(program_);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture.handle()));
  glUniform1i(sampler_, 0);
  glUniform1f(blue_first_, sampling.blue_first ? 1.0f : 0.0f);
  glUniform1f(top_down_, sampling.top_down ? 1.0f : 0.0f);
  glUniform1f(rotation_, static_cast<float>(sampling.rotation));
  glUniform4f(source_rect_, sampling.source_left, sampling.source_top,
              sampling.source_right, sampling.source_bottom);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), kQuad);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat),
                        kQuad + 2);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  return GlStatus("Draw GLES2 texture");
}

void Gles2TexturePresenter::Close() {
  if (program_ != 0) {
    glDeleteProgram(program_);
    program_ = 0;
  }
  sampler_ = -1;
  blue_first_ = -1;
  top_down_ = -1;
  rotation_ = -1;
  source_rect_ = -1;
}

}  // namespace symbian::api::display
