#include "renderer.h"

#include <e32debug.h>

namespace gl_app {
namespace {
TInt EglFailure() {
  RDebug::Print(_L("gl_app EGL error: %x"), eglGetError());
  return KErrNotSupported;
}
}  // namespace

Renderer::~Renderer() {
  if (display_ != EGL_NO_DISPLAY) {
    if (current_) {
      cube_.Close();
      exit_button_.Close();
    }
    eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (context_ != EGL_NO_CONTEXT) {
      eglDestroyContext(display_, context_);
    }
    if (surface_ != EGL_NO_SURFACE) {
      eglDestroySurface(display_, surface_);
    }
    eglTerminate(display_);
  }
}

TInt Renderer::Open(RWindow& window) {
  display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  EGLint major = 0, minor = 0;
  if (display_ == EGL_NO_DISPLAY || !eglInitialize(display_, &major, &minor)) {
    return EglFailure();
  }
  if (!eglBindAPI(EGL_OPENGL_ES_API)) {
    return EglFailure();
  }
  const EGLint attributes[] = {EGL_SURFACE_TYPE,
                               EGL_WINDOW_BIT,
                               EGL_RENDERABLE_TYPE,
                               EGL_OPENGL_ES2_BIT,
                               EGL_RED_SIZE,
                               8,
                               EGL_GREEN_SIZE,
                               8,
                               EGL_BLUE_SIZE,
                               8,
                               EGL_DEPTH_SIZE,
                               16,
                               EGL_NONE};
  EGLConfig config;
  EGLint count = 0;
  if (!eglChooseConfig(display_, attributes, &config, 1, &count) || !count) {
    return EglFailure();
  }
  // Symbian EGLNativeWindowType is a pointer to the actual RWindow object,
  // not its integer handle, and must remain alive through surface teardown.
  surface_ = eglCreateWindowSurface(display_, config, &window, nullptr);
  const EGLint context_attributes[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
  context_ =
      eglCreateContext(display_, config, EGL_NO_CONTEXT, context_attributes);
  if (surface_ == EGL_NO_SURFACE || context_ == EGL_NO_CONTEXT ||
      !eglMakeCurrent(display_, surface_, surface_, context_)) {
    return EglFailure();
  }
  current_ = true;
  if (!cube_.Open() || !exit_button_.Open()) {
    return KErrNotSupported;
  }
  return KErrNone;
}

TInt Renderer::Draw(float angle) {
  glViewport(0, 0, size_.iWidth, size_.iHeight);
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  cube_.Draw(size_, angle);
  exit_button_.Draw(size_);
  const GLenum error = glGetError();
  if (error != GL_NO_ERROR) {
    RDebug::Print(_L("gl_app GL error: %x"), error);
    return KErrGeneral;
  }
  if (!eglSwapBuffers(display_, surface_)) {
    return EglFailure();
  }
  return KErrNone;
}

}  // namespace gl_app
