#ifndef SYMBIAN_GL_APP_EXIT_BUTTON_H_
#define SYMBIAN_GL_APP_EXIT_BUTTON_H_

#include <w32std.h>

#include "symbian/api/display/gles_rect_batch.h"
#include "symbian/api/display/window_surface.h"

namespace gl_app {
class ExitButton {
 public:
  ExitButton() = default;
  ExitButton(const ExitButton&) = delete;
  ExitButton& operator=(const ExitButton&) = delete;
  void Draw(TSize size,
            symbian::api::display::GlesRectBatch* absl_nonnull batch);
  // Returns true only for a release inside after a press inside the button.
  bool HandlePointer(const symbian::api::display::WindowInput& pointer,
                     TSize size);
  static TRect Bounds(TSize size);

 private:
  void Rect(int x, int y, int width, int height, float shade);
  symbian::api::display::GlesRectBatch* absl_nullable batch_ = nullptr;
  TSize size_;
  bool pressed_ = false;
};
}  // namespace gl_app

#endif  // SYMBIAN_GL_APP_EXIT_BUTTON_H_
