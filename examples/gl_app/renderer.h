#ifndef SYMBIAN_GL_APP_RENDERER_H_
#define SYMBIAN_GL_APP_RENDERER_H_

#include <cstdint>

#include <absl/status/status.h>
#include <w32std.h>

#include "cube.h"
#include "exit_button.h"
#include "pause_panel.h"
#include "symbian/api/display/gles_rect_batch.h"
#include "symbian/api/display/gles_window_context.h"

namespace gl_app {
// Destroys GL objects before the SDK-owned context and window are closed.
class Renderer {
 public:
  Renderer() = default;
  ~Renderer();
  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;
  absl::Status Open(symbian::api::display::WindowSurface* absl_nonnull window);

  void Resize(TSize size) { size_ = size; }

  bool HandlePointer(const symbian::api::display::WindowInput& pointer) {
    return exit_button_.HandlePointer(pointer, size_);
  }

  PausePanel::Action HandlePausePointer(
      const symbian::api::display::WindowInput& pointer) {
    return pause_panel_.HandlePointer(pointer, size_);
  }

  absl::Status Draw(float yaw, float pitch, bool paused,
                    std::uint32_t frames_per_second);

 private:
  symbian::api::display::GlesWindowContext context_;
  bool objects_open_ = false;
  TSize size_;
  Cube cube_;
  symbian::api::display::GlesRectBatch ui_batch_;
  ExitButton exit_button_;
  PausePanel pause_panel_;
};
}  // namespace gl_app

#endif  // SYMBIAN_GL_APP_RENDERER_H_
