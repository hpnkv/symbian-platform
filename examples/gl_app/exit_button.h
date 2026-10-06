#ifndef SYMBIAN_GL_APP_EXIT_BUTTON_H_
#define SYMBIAN_GL_APP_EXIT_BUTTON_H_

#include <GLES2/gl2.h>
#include <w32std.h>

namespace gl_app {
class ExitButton {
 public:
  ExitButton() = default;
  ExitButton(const ExitButton&) = delete;
  ExitButton& operator=(const ExitButton&) = delete;
  // GL methods require the owning context current.
  bool Open();
  void Close();
  void Draw(TSize size);
  // Returns true only for a release inside after a press inside the button.
  bool HandlePointer(const TPointerEvent& pointer, TSize size);
  static TRect Bounds(TSize size);

 private:
  void Rect(int x, int y, int width, int height, float shade);
  GLuint program_ = 0;
  GLint color_ = -1;
  TSize size_;
  bool pressed_ = false;
};
}  // namespace gl_app

#endif  // SYMBIAN_GL_APP_EXIT_BUTTON_H_
