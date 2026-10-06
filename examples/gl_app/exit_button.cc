#include "exit_button.h"

#include "shader.h"
#include "shaders.h"

namespace gl_app {

bool ExitButton::Open() {
  program_ = CreateProgram(shaders::kExitVertex, shaders::kExitFragment);
  if (!program_) {
    return false;
  }
  color_ = glGetUniformLocation(program_, "uColor");
  return true;
}

void ExitButton::Close() {
  if (program_) {
    glDeleteProgram(program_);
  }
  program_ = 0;
}

TRect ExitButton::Bounds(TSize size) {
  return TRect(size.iWidth / 8, size.iHeight - 100, size.iWidth * 7 / 8,
               size.iHeight - 36);
}

bool ExitButton::HandlePointer(const TPointerEvent& pointer, TSize size) {
  const bool inside = Bounds(size).Contains(pointer.iPosition);
  if (pointer.iType == TPointerEvent::EButton1Down) {
    pressed_ = inside;
  }
  if (pointer.iType != TPointerEvent::EButton1Up) {
    return false;
  }
  const bool exit = pressed_ && inside;
  pressed_ = false;
  return exit;
}

void ExitButton::Rect(int x, int y, int width, int height, float shade) {
  const float left = 2.0f * x / size_.iWidth - 1.0f;
  const float right = 2.0f * (x + width) / size_.iWidth - 1.0f;
  const float top = 1.0f - 2.0f * y / size_.iHeight;
  const float bottom = 1.0f - 2.0f * (y + height) / size_.iHeight;
  const GLfloat points[] = {left,  top, 0, left,  bottom, 0,
                            right, top, 0, right, bottom, 0};
  glUniform4f(color_, shade, shade, shade, 1);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, points);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void ExitButton::Draw(TSize size) {
  size_ = size;
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glUseProgram(program_);
  glEnableVertexAttribArray(0);
  const TRect bounds = Bounds(size);
  const int x = bounds.iTl.iX, y = bounds.iTl.iY;
  const int w = bounds.Width(), h = bounds.Height();
  // Silver bevels and outlined pixel lettering echo the reference menu.
  Rect(x + 3, y + 4, w, h, 0.04f);
  Rect(x, y, w, h, 0.12f);
  Rect(x, y, w, 2, 0.86f);
  Rect(x, y, 2, h, 0.70f);
  Rect(x, y + h - 2, w, 2, 0.40f);
  Rect(x + w - 2, y, 2, h, 0.48f);
  for (int row = 0; row < h - 6; row += 2) {
    const float shade = (pressed_ ? 0.16f : 0.34f) - 0.12f * row / h;
    Rect(x + 3, y + 3 + row, w - 6, 2, shade);
  }
  constexpr unsigned char glyphs[4][7] = {{31, 16, 16, 30, 16, 16, 31},  // E
                                          {17, 17, 10, 4, 10, 17, 17},   // X
                                          {31, 4, 4, 4, 4, 4, 31},       // I
                                          {31, 4, 4, 4, 4, 4, 4}};       // T
  const int scale = size_.iWidth >= 300 ? 4 : 2;
  const int left = x + (w - 23 * scale) / 2;
  const int top = y + (h - 7 * scale) / 2 + (pressed_ ? 1 : 0);
  for (int pass = 0; pass < 2; ++pass) {
    for (int letter = 0; letter < 4; ++letter) {
      for (int row = 0; row < 7; ++row) {
        for (int column = 0; column < 5; ++column) {
          if (glyphs[letter][row] & (1 << (4 - column))) {
            const int px = left + (letter * 6 + column) * scale;
            const int py = top + row * scale;
            if (pass == 0) {
              Rect(px - 1, py - 1, scale + 2, scale + 2, 0.0f);
            } else {
              Rect(px, py, scale, scale, 0.94f);
            }
          }
        }
      }
    }
  }
  glDisableVertexAttribArray(0);
}

}  // namespace gl_app
