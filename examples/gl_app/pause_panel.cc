#include "pause_panel.h"

namespace gl_app {
namespace {

const unsigned char* absl_nullable Glyph(char letter) {
  static const unsigned char a[] = {14, 17, 17, 31, 17, 17, 17};
  static const unsigned char d[] = {30, 17, 17, 17, 17, 17, 30};
  static const unsigned char digits[10][7] = {
      {14, 17, 19, 21, 25, 17, 14}, {4, 12, 4, 4, 4, 4, 14},
      {14, 17, 1, 2, 4, 8, 31},     {30, 1, 1, 14, 1, 1, 30},
      {2, 6, 10, 18, 31, 2, 2},     {31, 16, 30, 1, 1, 17, 14},
      {6, 8, 16, 30, 17, 17, 14},   {31, 1, 2, 4, 8, 8, 8},
      {14, 17, 17, 14, 17, 17, 14}, {14, 17, 17, 15, 1, 2, 12}};
  static const unsigned char e[] = {31, 16, 16, 30, 16, 16, 31};
  static const unsigned char f[] = {31, 16, 16, 30, 16, 16, 16};
  static const unsigned char i[] = {31, 4, 4, 4, 4, 4, 31};
  static const unsigned char m[] = {17, 27, 21, 21, 17, 17, 17};
  static const unsigned char p[] = {30, 17, 17, 30, 16, 16, 16};
  static const unsigned char r[] = {30, 17, 17, 30, 20, 18, 17};
  static const unsigned char s[] = {15, 16, 16, 14, 1, 1, 30};
  static const unsigned char t[] = {31, 4, 4, 4, 4, 4, 4};
  static const unsigned char u[] = {17, 17, 17, 17, 17, 17, 14};
  static const unsigned char x[] = {17, 17, 10, 4, 10, 17, 17};
  switch (letter) {
    case 'A':
      return a;
    case 'D':
      return d;
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      return digits[letter - '0'];
    case 'E':
      return e;
    case 'F':
      return f;
    case 'I':
      return i;
    case 'M':
      return m;
    case 'P':
      return p;
    case 'R':
      return r;
    case 'S':
      return s;
    case 'T':
      return t;
    case 'U':
      return u;
    case 'X':
      return x;
    default:
      return nullptr;
  }
}

}  // namespace

TRect PausePanel::ButtonBounds(TSize size, int index) {
  const int width = size.iWidth * 3 / 4;
  const int left = (size.iWidth - width) / 2;
  const int top = size.iHeight / 2 - 18 + index * 72;
  return TRect(left, top, left + width, top + 56);
}

PausePanel::Action PausePanel::HandlePointer(
    const symbian::api::display::WindowInput& input, TSize size) {
  const TPoint point(input.x, input.y);
  if (input.kind == symbian::api::display::WindowInputKind::kPointerDown) {
    pressed_ = ButtonBounds(size, 0).Contains(point)   ? 0
               : ButtonBounds(size, 1).Contains(point) ? 1
                                                       : -1;
    return Action::kNone;
  }
  if (input.kind != symbian::api::display::WindowInputKind::kPointerUp) {
    return Action::kNone;
  }
  const int selected = pressed_;
  pressed_ = -1;
  if (selected < 0 || !ButtonBounds(size, selected).Contains(point)) {
    return Action::kNone;
  }
  return selected == 0 ? Action::kResume : Action::kExit;
}

void PausePanel::Rect(int x, int y, int width, int height, float red,
                      float green, float blue) {
  batch_->AddRect(x, y, width, height, red, green, blue);
}

void PausePanel::Label(const char* absl_nonnull text, int center_x, int top,
                       int scale) {
  int length = 0;
  while (text[length] != '\0') {
    ++length;
  }
  const int left = center_x - (length * 6 - 1) * scale / 2;
  for (int letter = 0; letter < length; ++letter) {
    const unsigned char* absl_nullable glyph = Glyph(text[letter]);
    if (glyph == nullptr) {
      continue;
    }
    for (int row = 0; row < 7; ++row) {
      for (int column = 0; column < 5; ++column) {
        if ((glyph[row] & (1 << (4 - column))) != 0) {
          Rect(left + (letter * 6 + column) * scale, top + row * scale, scale,
               scale, 0.97f, 0.98f, 1.0f);
        }
      }
    }
  }
}

void PausePanel::Draw(
    TSize size, symbian::api::display::GlesRectBatch* absl_nonnull batch) {
  batch_ = batch;
  const int width = size.iWidth * 7 / 8;
  const int left = (size.iWidth - width) / 2;
  const int top = size.iHeight / 2 - 84;
  Rect(left + 4, top + 5, width, 235, 0.0f, 0.0f, 0.0f);
  Rect(left, top, width, 235, 0.06f, 0.16f, 0.27f);
  Rect(left + 2, top + 2, width - 4, 2, 0.60f, 0.76f, 0.86f);
  const int heading_scale = size.iWidth >= 300 ? 3 : 2;
  Label("PAUSED", size.iWidth / 2, top + 16, heading_scale);
  for (int index = 0; index < 2; ++index) {
    const TRect bounds = ButtonBounds(size, index);
    const float shade = pressed_ == index ? 0.28f : 0.40f;
    Rect(bounds.iTl.iX, bounds.iTl.iY, bounds.Width(), bounds.Height(), 0.08f,
         shade, 0.62f);
    Rect(bounds.iTl.iX, bounds.iTl.iY, bounds.Width(), 2, 0.72f, 0.84f, 0.92f);
    const int scale = size.iWidth >= 300 ? 3 : 2;
    Label(index == 0 ? "RESUME" : "EXIT", size.iWidth / 2,
          bounds.iTl.iY + (bounds.Height() - 7 * scale) / 2, scale);
  }
  batch_ = nullptr;
}

void PausePanel::DrawFrameRate(
    TSize size, std::uint32_t frames_per_second,
    symbian::api::display::GlesRectBatch* absl_nonnull batch) {
  batch_ = batch;
  const std::uint32_t shown = frames_per_second > 999 ? 999 : frames_per_second;
  char label[] = "000 FPS";
  label[0] = static_cast<char>('0' + shown / 100);
  label[1] = static_cast<char>('0' + (shown / 10) % 10);
  label[2] = static_cast<char>('0' + shown % 10);
  Rect(size.iWidth - 51, 3, 48, 11, 0.06f, 0.16f, 0.27f);
  Label(label, size.iWidth - 27, 5, 1);
  batch_ = nullptr;
}

}  // namespace gl_app
