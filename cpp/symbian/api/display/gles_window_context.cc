// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "symbian/api/display/gles_window_context.h"

#include <cstring>
#include <new>
#include <string>

#include <EGL/egl.h>
#include <e32std.h>

namespace symbian::api::display {
namespace {

const char* absl_nonnull EglErrorName(EGLint error) {
  switch (error) {
    case EGL_BAD_ACCESS:
      return "BAD_ACCESS";
    case EGL_BAD_ALLOC:
      return "BAD_ALLOC";
    case EGL_BAD_ATTRIBUTE:
      return "BAD_ATTRIBUTE";
    case EGL_BAD_CONFIG:
      return "BAD_CONFIG";
    case EGL_BAD_CONTEXT:
      return "BAD_CONTEXT";
    case EGL_BAD_CURRENT_SURFACE:
      return "BAD_CURRENT_SURFACE";
    case EGL_BAD_DISPLAY:
      return "BAD_DISPLAY";
    case EGL_BAD_MATCH:
      return "BAD_MATCH";
    case EGL_BAD_NATIVE_PIXMAP:
      return "BAD_NATIVE_PIXMAP";
    case EGL_BAD_NATIVE_WINDOW:
      return "BAD_NATIVE_WINDOW";
    case EGL_BAD_PARAMETER:
      return "BAD_PARAMETER";
    case EGL_BAD_SURFACE:
      return "BAD_SURFACE";
    case EGL_CONTEXT_LOST:
      return "CONTEXT_LOST";
    default:
      return "UNKNOWN";
  }
}

absl::Status EglFailure(const char* absl_nonnull stage) {
  const EGLint error = eglGetError();
  std::string message(stage);
  message += ": ";
  message += EglErrorName(error);
  return absl::UnavailableError(message);
}

}  // namespace

struct GlesWindowContext::Impl {
  EGLDisplay display = EGL_NO_DISPLAY;
  EGLSurface surface = EGL_NO_SURFACE;
  EGLContext context = EGL_NO_CONTEXT;
  bool initialized = false;
  bool current = false;
  bool measure_frames = false;
  int swap_interval = 0;
  GlesFrameMetrics metrics;

  void Close() {
    if (display != EGL_NO_DISPLAY) {
      if (current) {
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
      }
      if (context != EGL_NO_CONTEXT) {
        eglDestroyContext(display, context);
      }
      if (surface != EGL_NO_SURFACE) {
        eglDestroySurface(display, surface);
      }
      if (initialized) {
        eglTerminate(display);
      }
    }
    display = EGL_NO_DISPLAY;
    surface = EGL_NO_SURFACE;
    context = EGL_NO_CONTEXT;
    initialized = false;
    current = false;
    swap_interval = 0;
  }
};

GlesWindowContext::GlesWindowContext() : impl_(new (std::nothrow) Impl) {}

GlesWindowContext::~GlesWindowContext() {
  Close();
  delete impl_;
}

absl::Status GlesWindowContext::Open(WindowSurface* absl_nonnull window,
                                     int major_version,
                                     GlesContextFormat format) {
  if (impl_ == nullptr) {
    return absl::ResourceExhaustedError("EGL owner allocation failed");
  }
  if (is_open()) {
    return absl::FailedPreconditionError("EGL context already open");
  }
  void* absl_nullable native_window = window->NativeWindowHandle();
  if (native_window == nullptr || major_version != 2 || format.red_bits < 0 ||
      format.green_bits < 0 || format.blue_bits < 0 || format.depth_bits < 0) {
    return absl::InvalidArgumentError("open window and OpenGL ES 2 required");
  }
  impl_->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  EGLint major = 0;
  EGLint minor = 0;
  if (impl_->display == EGL_NO_DISPLAY ||
      !eglInitialize(impl_->display, &major, &minor)) {
    const absl::Status error = EglFailure("EGL display");
    impl_->Close();
    return error;
  }
  impl_->initialized = true;
  if (!eglBindAPI(EGL_OPENGL_ES_API)) {
    const absl::Status error = EglFailure("EGL API");
    impl_->Close();
    return error;
  }
  const EGLint attributes[] = {EGL_SURFACE_TYPE,
                               EGL_WINDOW_BIT,
                               EGL_RENDERABLE_TYPE,
                               EGL_OPENGL_ES2_BIT,
                               EGL_RED_SIZE,
                               format.red_bits,
                               EGL_GREEN_SIZE,
                               format.green_bits,
                               EGL_BLUE_SIZE,
                               format.blue_bits,
                               EGL_DEPTH_SIZE,
                               format.depth_bits,
                               EGL_NONE};
  EGLConfig config = 0;
  EGLint count = 0;
  if (!eglChooseConfig(impl_->display, attributes, &config, 1, &count) ||
      count == 0) {
    const absl::Status error =
        count == 0 ? absl::UnavailableError("EGL config: no ES2 window format")
                   : EglFailure("EGL config");
    impl_->Close();
    return error;
  }
  impl_->surface =
      eglCreateWindowSurface(impl_->display, config, native_window, nullptr);
  if (impl_->surface == EGL_NO_SURFACE) {
    const absl::Status error = EglFailure("EGL surface");
    impl_->Close();
    return error;
  }
  const EGLint context_attributes[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
  impl_->context = eglCreateContext(impl_->display, config, EGL_NO_CONTEXT,
                                    context_attributes);
  if (impl_->context == EGL_NO_CONTEXT) {
    const absl::Status error = EglFailure("EGL context");
    impl_->Close();
    return error;
  }
  if (!eglMakeCurrent(impl_->display, impl_->surface, impl_->surface,
                      impl_->context)) {
    const absl::Status error = EglFailure("EGL current");
    impl_->Close();
    return error;
  }
  impl_->current = true;
  return absl::OkStatus();
}

absl::Status GlesWindowContext::MakeCurrent() {
  if (!is_open()) {
    return absl::FailedPreconditionError("EGL context is closed");
  }
  if (!eglMakeCurrent(impl_->display, impl_->surface, impl_->surface,
                      impl_->context)) {
    return absl::UnavailableError("EGL make-current failed");
  }
  impl_->current = true;
  return absl::OkStatus();
}

absl::Status GlesWindowContext::ClearCurrent() {
  if (!is_open()) {
    return absl::FailedPreconditionError("EGL context is closed");
  }
  if (!eglMakeCurrent(impl_->display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                      EGL_NO_CONTEXT)) {
    return absl::UnavailableError("EGL clear-current failed");
  }
  impl_->current = false;
  return absl::OkStatus();
}

absl::Status GlesWindowContext::Swap() {
  if (!is_open()) {
    return absl::FailedPreconditionError("EGL context is closed");
  }
  const std::uint32_t before = impl_->measure_frames ? User::NTickCount() : 0;
  if (!eglSwapBuffers(impl_->display, impl_->surface)) {
    return absl::UnavailableError("EGL swap failed");
  }
  if (impl_->measure_frames) {
    const std::uint32_t elapsed = User::NTickCount() - before;
    ++impl_->metrics.frames;
    impl_->metrics.total_native_ticks += elapsed;
    if (elapsed > impl_->metrics.max_native_ticks) {
      impl_->metrics.max_native_ticks = elapsed;
    }
  }
  return absl::OkStatus();
}

absl::Status GlesWindowContext::SetSwapInterval(int interval) {
  if (!is_open()) {
    return absl::FailedPreconditionError("EGL context is closed");
  }
  if (interval < 0) {
    return absl::InvalidArgumentError("negative swap interval unsupported");
  }
  if (!eglSwapInterval(impl_->display, interval)) {
    return absl::UnavailableError("EGL swap interval unavailable");
  }
  impl_->swap_interval = interval;
  return absl::OkStatus();
}

int GlesWindowContext::swap_interval() const {
  return impl_ == nullptr ? 0 : impl_->swap_interval;
}

bool GlesWindowContext::is_open() const {
  return impl_ != nullptr && impl_->context != EGL_NO_CONTEXT;
}

void GlesWindowContext::set_measure_frames(bool enabled) {
  if (impl_ != nullptr) {
    impl_->measure_frames = enabled;
  }
}

GlesFrameMetrics GlesWindowContext::frame_metrics() const {
  return impl_ == nullptr ? GlesFrameMetrics{} : impl_->metrics;
}

void GlesWindowContext::Close() {
  if (impl_ != nullptr) {
    impl_->Close();
  }
}

GlesWindowContext::Procedure absl_nullable GlesWindowContext::GetProcAddress(
    std::string_view name) {
  if (name.empty() || name.size() >= 128) {
    return nullptr;
  }
  char terminated[128];
  std::memcpy(terminated, name.data(), name.size());
  terminated[name.size()] = '\0';
  return eglGetProcAddress(terminated);
}

}  // namespace symbian::api::display
