#ifndef SYMBIAN_GL_APP_RENDERER_H_
#define SYMBIAN_GL_APP_RENDERER_H_

#include <EGL/egl.h>
#include <w32std.h>

#include "cube.h"
#include "exit_button.h"

namespace gl_app {
// Owns EGL and destroys GL objects before detaching the context. The caller
// keeps the Window Server session and RWindow alive through destruction.
class Renderer {
 public:
  Renderer() = default;
  ~Renderer();
  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;
  TInt Open(RWindow& window);

  void Resize(TSize size) { size_ = size; }

  bool HandlePointer(const TPointerEvent& pointer) {
    return exit_button_.HandlePointer(pointer, size_);
  }

  TInt Draw(float angle);

 private:
  EGLDisplay display_ = EGL_NO_DISPLAY;
  EGLSurface surface_ = EGL_NO_SURFACE;
  EGLContext context_ = EGL_NO_CONTEXT;
  bool current_ = false;
  TSize size_;
  Cube cube_;
  ExitButton exit_button_;
};
}  // namespace gl_app

#endif  // SYMBIAN_GL_APP_RENDERER_H_
