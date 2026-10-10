// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "symbian/api/camera/gles2_frame_backend.h"

#include <cstring>
#include <memory>
#include <new>
#include <optional>
#include <utility>

#include <GLES2/gl2.h>

namespace symbian::api::camera {
namespace {

constexpr char kVertexShader[] =
    "attribute vec2 aPosition;\n"
    "attribute vec2 aTexCoord;\n"
    "varying mediump vec2 vTexCoord;\n"
    "void main() { vTexCoord = aTexCoord; "
    "gl_Position = vec4(aPosition, 0.0, 1.0); }\n";
constexpr char kFragmentShader[] =
    "precision mediump float;\n"
    "varying mediump vec2 vTexCoord;\n"
    "uniform sampler2D uTexture;\n"
    "uniform float uOpaque; "
    "uniform float uGray;\n"
    "uniform float uBgrx; uniform float uTopDown; "
    "void main() { vec2 coord = vec2(vTexCoord.x, "
    "mix(vTexCoord.y, 1.0 - vTexCoord.y, uTopDown)); "
    "vec4 c = texture2D(uTexture, coord); "
    "vec3 rgb = mix(mix(c.rgb, c.bgr, uBgrx), vec3(c.r), uGray); "
    "gl_FragColor = vec4(rgb, mix(c.a, 1.0, uOpaque)); }\n";
constexpr GLfloat kQuad[] = {
    -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, 0.0f,
    -1.0f, 1.0f,  0.0f, 1.0f, 1.0f, 1.0f,  1.0f, 1.0f,
};

bool IsMemory(MemoryKind kind) {
  return kind == MemoryKind::kLinear || kind == MemoryKind::kMapped;
}

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

struct AttributeState {
  GLint enabled = 0;
  GLint size = 0;
  GLint type = 0;
  GLint normalized = 0;
  GLint stride = 0;
  GLint buffer = 0;
  void* absl_nullable pointer = nullptr;

  explicit AttributeState(GLuint index) {
    glGetVertexAttribiv(index, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &enabled);
    glGetVertexAttribiv(index, GL_VERTEX_ATTRIB_ARRAY_SIZE, &size);
    glGetVertexAttribiv(index, GL_VERTEX_ATTRIB_ARRAY_TYPE, &type);
    glGetVertexAttribiv(index, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &normalized);
    glGetVertexAttribiv(index, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &stride);
    glGetVertexAttribiv(index, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &buffer);
    glGetVertexAttribPointerv(index, GL_VERTEX_ATTRIB_ARRAY_POINTER, &pointer);
  }

  void Restore(GLuint index) const {
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glVertexAttribPointer(index, size, type, normalized, stride, pointer);
    if (enabled != 0) {
      glEnableVertexAttribArray(index);
    } else {
      glDisableVertexAttribArray(index);
    }
  }
};

struct GlStateGuard {
  GLint framebuffer = 0;
  GLint viewport[4] = {};
  GLint program = 0;
  GLint active_texture = 0;
  GLint texture0 = 0;
  GLint array_buffer = 0;
  GLint unpack_alignment = 0;
  GLint pack_alignment = 0;
  GLboolean blend = GL_FALSE;
  GLboolean depth = GL_FALSE;
  GLboolean cull = GL_FALSE;
  GLboolean scissor = GL_FALSE;
  GLboolean stencil = GL_FALSE;
  GLboolean color_mask[4] = {};
  AttributeState position{0};
  AttributeState texture{1};

  GlStateGuard() {
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &active_texture);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture0);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &array_buffer);
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpack_alignment);
    glGetIntegerv(GL_PACK_ALIGNMENT, &pack_alignment);
    blend = glIsEnabled(GL_BLEND);
    depth = glIsEnabled(GL_DEPTH_TEST);
    cull = glIsEnabled(GL_CULL_FACE);
    scissor = glIsEnabled(GL_SCISSOR_TEST);
    stencil = glIsEnabled(GL_STENCIL_TEST);
    glGetBooleanv(GL_COLOR_WRITEMASK, color_mask);
  }

  ~GlStateGuard() {
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    glUseProgram(program);
    position.Restore(0);
    texture.Restore(1);
    glBindBuffer(GL_ARRAY_BUFFER, array_buffer);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture0);
    glActiveTexture(active_texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, unpack_alignment);
    glPixelStorei(GL_PACK_ALIGNMENT, pack_alignment);
    glColorMask(color_mask[0], color_mask[1], color_mask[2], color_mask[3]);
    blend ? glEnable(GL_BLEND) : glDisable(GL_BLEND);
    depth ? glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST);
    cull ? glEnable(GL_CULL_FACE) : glDisable(GL_CULL_FACE);
    scissor ? glEnable(GL_SCISSOR_TEST) : glDisable(GL_SCISSOR_TEST);
    stencil ? glEnable(GL_STENCIL_TEST) : glDisable(GL_STENCIL_TEST);
  }
};

absl::Status GlStatus(const char* absl_nonnull operation) {
  return glGetError() == GL_NO_ERROR ? absl::OkStatus()
                                     : absl::UnavailableError(operation);
}

absl::Status UploadRows(const FrameView& source, GLuint texture,
                        std::unique_ptr<std::byte[]>* absl_nonnull staging,
                        std::size_t* absl_nonnull staging_capacity) {
  const bool rgb565 = source.layout.format == PixelFormat::kRgb565;
  const bool bgr565 = source.layout.format == PixelFormat::kBgr565;
  const bool bgrx = source.layout.format == PixelFormat::kBgrx8888;
  const GLenum format = source.layout.format == PixelFormat::kGray8
                            ? GL_LUMINANCE
                            : (rgb565 ? GL_RGB : GL_RGBA);
  const GLenum type = rgb565 ? GL_UNSIGNED_SHORT_5_6_5 : GL_UNSIGNED_BYTE;
  const std::size_t row_bytes =
      static_cast<std::size_t>(source.layout.width) *
      (bgr565 || bgrx || source.layout.format == PixelFormat::kRgba8888 ||
               source.layout.format == PixelFormat::kRgbx8888
           ? 4
           : (rgb565 ? 2 : 1));
  const std::size_t bytes = row_bytes * source.layout.height;
  if (bgrx && source.layout.stride_bytes[0] == row_bytes) {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, source.layout.width,
                    source.layout.height, GL_RGBA, GL_UNSIGNED_BYTE,
                    source.planes[0].data());
    return GlStatus("GLES2 direct camera upload");
  }
  if (*staging_capacity < bytes) {
    std::unique_ptr<std::byte[]> replacement(new (std::nothrow)
                                                 std::byte[bytes]);
    if (replacement == nullptr) {
      return absl::ResourceExhaustedError("GLES2 upload staging allocation");
    }
    *staging = std::move(replacement);
    *staging_capacity = bytes;
  }
  for (int row = 0; row < source.layout.height; ++row) {
    const std::byte* absl_nonnull input =
        source.planes[0].data() +
        static_cast<std::size_t>(row) * source.layout.stride_bytes[0];
    std::byte* absl_nonnull output =
        staging->get() +
        static_cast<std::size_t>(bgrx ? row : source.layout.height - row - 1) *
            row_bytes;
    if (bgr565) {
      for (int x = 0; x < source.layout.width; ++x) {
        const unsigned packed =
            std::to_integer<unsigned char>(input[x * 2]) |
            (std::to_integer<unsigned char>(input[x * 2 + 1]) << 8);
        output[x * 4] = static_cast<std::byte>(
            (((packed & 31) << 3) | ((packed & 31) >> 2)));
        output[x * 4 + 1] = static_cast<std::byte>(
            ((((packed >> 5) & 63) << 2) | (((packed >> 5) & 63) >> 4)));
        output[x * 4 + 2] = static_cast<std::byte>(
            ((((packed >> 11) & 31) << 3) | (((packed >> 11) & 31) >> 2)));
        output[x * 4 + 3] = std::byte{255};
      }
    } else {
      std::memcpy(output, input, row_bytes);
    }
  }
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, source.layout.width,
                  source.layout.height, format, type, staging->get());
  return GlStatus("GLES2 camera upload");
}

absl::Status DownloadRows(const MutableFrameView& destination) {
  const std::size_t row_bytes =
      static_cast<std::size_t>(destination.layout.width) * 4;
  const std::size_t bytes =
      row_bytes * static_cast<std::size_t>(destination.layout.height);
  std::unique_ptr<std::byte[]> staging(new (std::nothrow) std::byte[bytes]);
  if (staging == nullptr) {
    return absl::ResourceExhaustedError("GLES2 camera readback allocation");
  }
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, destination.layout.width, destination.layout.height,
               GL_RGBA, GL_UNSIGNED_BYTE, staging.get());
  absl::Status result = GlStatus("GLES2 camera readback");
  if (!result.ok()) {
    return result;
  }
  for (int row = 0; row < destination.layout.height; ++row) {
    std::byte* absl_nonnull output =
        destination.planes[0].data() +
        static_cast<std::size_t>(row) * destination.layout.stride_bytes[0];
    std::memcpy(output,
                staging.get() + static_cast<std::size_t>(
                                    destination.layout.height - row - 1) *
                                    row_bytes,
                row_bytes);
  }
  return absl::OkStatus();
}

}  // namespace

struct Gles2FrameBackend::Impl {
  GLuint program = 0;
  GLuint framebuffer = 0;
  GLint sampler = -1;
  GLint opaque = -1;
  GLint gray = -1;
  GLint bgrx = -1;
  GLint top_down = -1;
  std::unique_ptr<std::byte[]> upload_staging;
  std::size_t upload_staging_capacity = 0;

  absl::Status Open() {
    const GLuint vertex = Compile(GL_VERTEX_SHADER, kVertexShader);
    const GLuint fragment = Compile(GL_FRAGMENT_SHADER, kFragmentShader);
    if (vertex == 0 || fragment == 0) {
      if (vertex != 0) {
        glDeleteShader(vertex);
      }
      if (fragment != 0) {
        glDeleteShader(fragment);
      }
      return absl::UnavailableError("GLES2 camera shaders");
    }
    program = glCreateProgram();
    if (program != 0) {
      glAttachShader(program, vertex);
      glAttachShader(program, fragment);
      glBindAttribLocation(program, 0, "aPosition");
      glBindAttribLocation(program, 1, "aTexCoord");
      glLinkProgram(program);
    }
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    GLint linked = 0;
    if (program != 0) {
      glGetProgramiv(program, GL_LINK_STATUS, &linked);
    }
    if (linked == 0) {
      Close();
      return absl::UnavailableError("GLES2 camera program");
    }
    sampler = glGetUniformLocation(program, "uTexture");
    opaque = glGetUniformLocation(program, "uOpaque");
    gray = glGetUniformLocation(program, "uGray");
    bgrx = glGetUniformLocation(program, "uBgrx");
    top_down = glGetUniformLocation(program, "uTopDown");
    glGenFramebuffers(1, &framebuffer);
    if (framebuffer == 0) {
      Close();
      return absl::UnavailableError("GLES2 camera framebuffer");
    }
    return absl::OkStatus();
  }

  absl::Status Draw(GLuint source, GLuint destination, int width, int height,
                    ResampleFilter filter, bool force_opaque, bool grayscale,
                    bool blue_first_top_down) {
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           destination, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
      return absl::UnavailableError(
          "GLES2 destination texture is not renderable");
    }
    glViewport(0, 0, width, height);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_STENCIL_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glUseProgram(program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, source);
    GLint old_min = 0;
    GLint old_mag = 0;
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &old_min);
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &old_mag);
    const GLint sample_filter =
        filter == ResampleFilter::kNearest ? GL_NEAREST : GL_LINEAR;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, sample_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, sample_filter);
    glUniform1i(sampler, 0);
    glUniform1f(opaque, force_opaque ? 1.0f : 0.0f);
    glUniform1f(gray, grayscale ? 1.0f : 0.0f);
    glUniform1f(bgrx, blue_first_top_down ? 1.0f : 0.0f);
    glUniform1f(top_down, blue_first_top_down ? 1.0f : 0.0f);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), kQuad);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat),
                          kQuad + 2);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, old_min);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, old_mag);
    return GlStatus("GLES2 camera texture transform");
  }

  void Close() {
    if (framebuffer != 0) {
      glDeleteFramebuffers(1, &framebuffer);
      framebuffer = 0;
    }
    if (program != 0) {
      glDeleteProgram(program);
      program = 0;
    }
  }
};

Gles2FrameBackend::Gles2FrameBackend(std::uintptr_t device,
                                     Gles2ContextUse context_use)
    : device_(device), context_use_(context_use) {}

Gles2FrameBackend::~Gles2FrameBackend() {
  Close();
}

FrameBackend Gles2FrameBackend::Borrow() {
  return {.transform = [this](const FrameView& source,
                              const MutableFrameView& destination,
                              ResampleFilter filter) {
    return Transform(source, destination, filter);
  }};
}

absl::Status Gles2FrameBackend::Transform(const FrameView& source,
                                          const MutableFrameView& destination,
                                          ResampleFilter filter) {
  const bool source_memory = IsMemory(source.memory);
  if (device_ == 0 ||
      (source.layout.format != PixelFormat::kRgba8888 &&
       !(source_memory && (source.layout.format == PixelFormat::kRgb565 ||
                           source.layout.format == PixelFormat::kBgr565 ||
                           source.layout.format == PixelFormat::kBgrx8888 ||
                           source.layout.format == PixelFormat::kRgbx8888 ||
                           source.layout.format == PixelFormat::kGray8))) ||
      (destination.layout.format != PixelFormat::kRgba8888 &&
       !(source_memory && source.layout.format == PixelFormat::kBgrx8888 &&
         destination.layout.format == PixelFormat::kBgrx8888 &&
         source.layout.width == destination.layout.width &&
         source.layout.height == destination.layout.height))) {
    return absl::UnimplementedError("GLES2 frame format is unsupported");
  }
  const bool destination_memory = IsMemory(destination.memory);
  if (source_memory && destination_memory) {
    return absl::UnimplementedError("Use TransformFrame for memory frames");
  }
  if ((!source_memory && (source.memory != MemoryKind::kGles2Texture ||
                          source.device != device_)) ||
      (!destination_memory &&
       (destination.memory != MemoryKind::kGles2Texture ||
        destination.device != device_))) {
    return absl::InvalidArgumentError("GLES2 frame belongs to another backend");
  }
  if (!source_memory && !destination_memory &&
      source.handle == destination.handle) {
    return source.layout.width == destination.layout.width &&
                   source.layout.height == destination.layout.height
               ? absl::OkStatus()
               : absl::InvalidArgumentError("Cannot resize a texture in place");
  }
  if (glGetError() != GL_NO_ERROR) {
    return absl::FailedPreconditionError("Existing GLES2 error");
  }
  std::optional<GlStateGuard> guard;
  if (context_use_ == Gles2ContextUse::kPreserveState) {
    guard.emplace();
    absl::Status state = GlStatus("GLES2 camera state query");
    if (!state.ok()) {
      return state;
    }
  }
  absl::Status result = absl::OkStatus();
  if (impl_ == nullptr) {
    impl_ = new (std::nothrow) Impl;
    if (impl_ == nullptr) {
      return absl::ResourceExhaustedError("GLES2 camera backend allocation");
    }
    absl::Status opened = impl_->Open();
    if (!opened.ok()) {
      return opened;
    }
    result = GlStatus("GLES2 camera setup");
    if (!result.ok()) {
      return result;
    }
  }
  GLuint temporary_source = 0;
  GLuint temporary_destination = 0;
  GLuint source_texture = static_cast<GLuint>(source.handle);
  GLuint destination_texture = static_cast<GLuint>(destination.handle);
  const bool same_size = source.layout.width == destination.layout.width &&
                         source.layout.height == destination.layout.height;
  const bool direct_upload =
      source_memory && !destination_memory && same_size &&
      ((source.layout.format == PixelFormat::kRgba8888 &&
        destination.layout.format == PixelFormat::kRgba8888) ||
       (source.layout.format == PixelFormat::kBgr565 &&
        destination.layout.format == PixelFormat::kRgba8888) ||
       (source.layout.format == PixelFormat::kBgrx8888 &&
        destination.layout.format == PixelFormat::kBgrx8888));
  const bool direct_readback =
      !source_memory && destination_memory && same_size;
  if (source_memory) {
    if (!direct_upload) {
      glGenTextures(1, &temporary_source);
      source_texture = temporary_source;
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, source_texture);
      const bool rgb565 = source.layout.format == PixelFormat::kRgb565;
      const GLenum format = source.layout.format == PixelFormat::kGray8
                                ? GL_LUMINANCE
                                : (rgb565 ? GL_RGB : GL_RGBA);
      glTexImage2D(GL_TEXTURE_2D, 0, format, source.layout.width,
                   source.layout.height, 0, format,
                   rgb565 ? GL_UNSIGNED_SHORT_5_6_5 : GL_UNSIGNED_BYTE,
                   nullptr);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    } else {
      source_texture = destination_texture;
    }
    result = UploadRows(source, source_texture, &impl_->upload_staging,
                        &impl_->upload_staging_capacity);
  }
  if (destination_memory) {
    if (!direct_readback) {
      glGenTextures(1, &temporary_destination);
      destination_texture = temporary_destination;
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, destination_texture);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, destination.layout.width,
                   destination.layout.height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                   nullptr);
    } else {
      destination_texture = source_texture;
    }
  }
  if (result.ok()) {
    result = GlStatus("GLES2 camera texture allocation or upload");
  }
  if (result.ok() && !direct_upload && !direct_readback) {
    result = impl_->Draw(
        source_texture, destination_texture, destination.layout.width,
        destination.layout.height, filter,
        source_memory && source.layout.format == PixelFormat::kRgbx8888,
        source_memory && source.layout.format == PixelFormat::kGray8,
        source_memory && source.layout.format == PixelFormat::kBgrx8888);
  }
  if (result.ok() && destination_memory) {
    glBindFramebuffer(GL_FRAMEBUFFER, impl_->framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           destination_texture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
      result = absl::UnavailableError("GLES2 source texture is not readable");
    } else {
      result = DownloadRows(destination);
    }
  }
  if (result.ok()) {
    // Client pixel data passed to glTexSubImage2D has been consumed when that
    // call returns. Texture work in this context remains ordered with the
    // caller's next draw; glReadPixels completes its RAM readback itself.
    // Cross-context use still requires a fence supplied by the caller.
    result = GlStatus("GLES2 camera completion");
  }
  if (temporary_source != 0) {
    glDeleteTextures(1, &temporary_source);
  }
  if (temporary_destination != 0) {
    glDeleteTextures(1, &temporary_destination);
  }
  return result;
}

void Gles2FrameBackend::Close() {
  if (impl_ != nullptr) {
    impl_->Close();
    delete impl_;
    impl_ = nullptr;
  }
}

}  // namespace symbian::api::camera
