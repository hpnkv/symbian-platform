// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include <absl/status/status_macros.h>
#include "symbian/api/display/gles_rect_batch.h"

#include <new>
#include <utility>

#include <GLES2/gl2.h>

namespace symbian::api::display {
namespace {

constexpr char kVertexShader[] =
    "attribute vec2 aPosition;\n"
    "attribute vec4 aColor;\n"
    "varying lowp vec4 vColor;\n"
    "void main() { vColor = aColor; gl_Position = vec4(aPosition, 0.0, 1.0); "
    "}\n";
constexpr char kFragmentShader[] =
    "precision lowp float;\n"
    "varying lowp vec4 vColor;\n"
    "void main() { gl_FragColor = vColor; }\n";

GLuint Compile(GLenum type, const char* absl_nonnull source) {
  const GLuint shader = glCreateShader(type);
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

}  // namespace

struct GlesRectBatch::Impl {
  struct Vertex {
    GLfloat position[2];
    GLfloat color[4];
  };

  static constexpr int kMaxRects = 256;
  Vertex vertices[kMaxRects * 6];
  GLuint program = 0;
  int width = 0;
  int height = 0;
  int rects = 0;

  void Draw() {
    if (rects == 0 || program == 0) {
      return;
    }
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glUseProgram(program);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          vertices[0].position);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          vertices[0].color);
    glDrawArrays(GL_TRIANGLES, 0, rects * 6);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(0);
    rects = 0;
  }
};

GlesRectBatch::GlesRectBatch() = default;

GlesRectBatch::GlesRectBatch(GlesRectBatch&& other) noexcept
    : impl_(std::exchange(other.impl_, nullptr)) {}

GlesRectBatch& GlesRectBatch::operator=(GlesRectBatch&& other) noexcept {
  if (this != &other) {
    Close();
    delete impl_;
    impl_ = std::exchange(other.impl_, nullptr);
  }
  return *this;
}

absl::StatusOr<GlesRectBatch> GlesRectBatch::Create() {
  GlesRectBatch result;
  ABSL_RETURN_IF_ERROR(result.Open());
  return result;
}

absl::StatusOr<std::unique_ptr<GlesRectBatch>> GlesRectBatch::CreateUnique() {
  ABSL_ASSIGN_OR_RETURN(auto created, Create());
  std::unique_ptr<GlesRectBatch> owner(new (std::nothrow)
                                           GlesRectBatch(std::move(created)));
  if (owner == nullptr) {
    return absl::ResourceExhaustedError("GLES batch owner allocation failed");
  }
  return owner;
}

GlesRectBatch::~GlesRectBatch() {
  Close();
  delete impl_;
}

absl::Status GlesRectBatch::Open() {
  if (impl_ == nullptr) {
    impl_ = new (std::nothrow) Impl;
    if (impl_ == nullptr) {
      return absl::ResourceExhaustedError("GLES rectangle batch allocation");
    }
  }
  if (impl_->program != 0) {
    return absl::FailedPreconditionError("GLES rectangle batch already open");
  }
  const GLuint vertex = Compile(GL_VERTEX_SHADER, kVertexShader);
  const GLuint fragment = Compile(GL_FRAGMENT_SHADER, kFragmentShader);
  if (vertex == 0 || fragment == 0) {
    if (vertex != 0) {
      glDeleteShader(vertex);
    }
    if (fragment != 0) {
      glDeleteShader(fragment);
    }
    return absl::UnavailableError("GLES rectangle batch shader");
  }
  impl_->program = glCreateProgram();
  if (impl_->program != 0) {
    glAttachShader(impl_->program, vertex);
    glAttachShader(impl_->program, fragment);
    glBindAttribLocation(impl_->program, 0, "aPosition");
    glBindAttribLocation(impl_->program, 1, "aColor");
    glLinkProgram(impl_->program);
  }
  glDeleteShader(vertex);
  glDeleteShader(fragment);
  GLint linked = 0;
  if (impl_->program != 0) {
    glGetProgramiv(impl_->program, GL_LINK_STATUS, &linked);
  }
  if (linked == 0) {
    Close();
    return absl::UnavailableError("GLES rectangle batch program");
  }
  return absl::OkStatus();
}

void GlesRectBatch::Begin(int width, int height) {
  if (impl_ == nullptr) {
    return;
  }
  impl_->rects = 0;
  impl_->width = width;
  impl_->height = height;
}

void GlesRectBatch::AddRect(int x, int y, int width, int height, float red,
                            float green, float blue, float alpha) {
  if (impl_ == nullptr || impl_->program == 0 || impl_->width <= 0 ||
      impl_->height <= 0 || width <= 0 || height <= 0) {
    return;
  }
  if (impl_->rects == Impl::kMaxRects) {
    impl_->Draw();
  }
  const float left = 2.0f * x / impl_->width - 1.0f;
  const float right = 2.0f * (x + width) / impl_->width - 1.0f;
  const float top = 1.0f - 2.0f * y / impl_->height;
  const float bottom = 1.0f - 2.0f * (y + height) / impl_->height;
  const GLfloat points[6][2] = {{left, top},  {left, bottom}, {right, top},
                                {right, top}, {left, bottom}, {right, bottom}};
  for (int vertex = 0; vertex < 6; ++vertex) {
    auto& output = impl_->vertices[impl_->rects * 6 + vertex];
    output.position[0] = points[vertex][0];
    output.position[1] = points[vertex][1];
    output.color[0] = red;
    output.color[1] = green;
    output.color[2] = blue;
    output.color[3] = alpha;
  }
  ++impl_->rects;
}

void GlesRectBatch::Draw() {
  if (impl_ != nullptr) {
    impl_->Draw();
  }
}

void GlesRectBatch::Close() {
  if (impl_ != nullptr && impl_->program != 0) {
    glDeleteProgram(impl_->program);
    impl_->program = 0;
    impl_->rects = 0;
  }
}

}  // namespace symbian::api::display
