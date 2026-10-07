#include "exit_button.h"

namespace gl_app {

TRect ExitButton::Bounds(TSize size) {
  return TRect(size.iWidth / 8, size.iHeight - 100, size.iWidth * 7 / 8,
               size.iHeight - 36);
}

bool ExitButton::HandlePointer(
    const symbian::api::display::WindowInput& pointer, TSize size) {
  const bool inside = Bounds(size).Contains(TPoint(pointer.x, pointer.y));
  if (pointer.kind == symbian::api::display::WindowInputKind::kPointerDown) {
    pressed_ = inside;
  }
  if (pointer.kind != symbian::api::display::WindowInputKind::kPointerUp) {
    return false;
  }
  const bool exit = pressed_ && inside;
  pressed_ = false;
  return exit;
}

void ExitButton::Rect(int x, int y, int width, int height, float shade) {
  batch_->AddRect(x, y, width, height, shade, shade, shade);
}

void ExitButton::Draw(
    TSize size, symbian::api::display::GlesRectBatch* absl_nonnull batch) {
  batch_ = batch;
  size_ = size;
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
  batch_ = nullptr;
}

}  // namespace gl_app
