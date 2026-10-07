#ifndef SYMBIAN_GL_APP_PAUSE_PANEL_H_
#define SYMBIAN_GL_APP_PAUSE_PANEL_H_

#include <cstdint>

#include <absl/base/nullability.h>
#include <w32std.h>

#include "symbian/api/display/gles_rect_batch.h"
#include "symbian/api/display/window_surface.h"

namespace gl_app {

class PausePanel final {
 public:
  enum class Action { kNone, kResume, kExit };
  void Draw(TSize size,
            symbian::api::display::GlesRectBatch* absl_nonnull batch);
  void DrawFrameRate(TSize size, std::uint32_t frames_per_second,
                     symbian::api::display::GlesRectBatch* absl_nonnull batch);
  Action HandlePointer(const symbian::api::display::WindowInput& input,
                       TSize size);

 private:
  static TRect ButtonBounds(TSize size, int index);
  void Rect(int x, int y, int width, int height, float red, float green,
            float blue);
  void Label(const char* absl_nonnull text, int center_x, int top, int scale);
  symbian::api::display::GlesRectBatch* absl_nullable batch_ = nullptr;
  int pressed_ = -1;
};

}  // namespace gl_app

#endif  // SYMBIAN_GL_APP_PAUSE_PANEL_H_
