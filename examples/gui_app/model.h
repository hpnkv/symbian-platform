#ifndef SYMBIAN_GUI_APP_MODEL_H_
#define SYMBIAN_GUI_APP_MODEL_H_

namespace gui_app {

struct QuotientRemainder {
  unsigned int quotient;
  unsigned int remainder;
};

// Nonzero divisor required. This small ARMv5 example deliberately does not
// depend on a compiler-rt division helper before that runtime is available.
constexpr QuotientRemainder Divide(unsigned int value, unsigned int divisor) {
  unsigned int quotient = 0;
  for (unsigned int bit = 32; bit != 0; --bit) {
    const unsigned int shift = bit - 1;
    if ((value >> shift) >= divisor) {
      value -= divisor << shift;
      quotient |= 1U << shift;
    }
  }
  return {quotient, value};
}

constexpr int DividePositive(int value, unsigned int divisor) {
  return static_cast<int>(
      Divide(static_cast<unsigned int>(value), divisor).quotient);
}

struct Rect {
  int x;
  int y;
  int width;
  int height;

  constexpr bool Contains(int px, int py) const {
    return px >= x && py >= y && px - x < width && py - y < height;
  }
};

struct Layout {
  Rect increment;
  Rect reset;
  Rect exit;
  int width;
  int height;
};

constexpr Layout MakeLayout(int width, int height) {
  // Small displays keep usable controls; extremely small ones are rejected by
  // the platform adapter before a window is created.
  const int gap = 12;
  const int button_width = DividePositive(width - 4 * gap, 3);
  const int button_height = DividePositive(height, 6);
  const int top = height - button_height - gap;
  return {{gap, top, button_width, button_height},
          {2 * gap + button_width, top, button_width, button_height},
          {3 * gap + 2 * button_width, top, button_width, button_height},
          width,
          height};
}

constexpr int DigitScale(const Layout& layout) {
  const int horizontal = DividePositive(layout.width, 28);
  const int vertical = DividePositive(
      layout.increment.y - DividePositive(layout.height, 4) - 12, 9);
  return horizontal < vertical ? horizontal : vertical;
}

class Model {
 public:
  constexpr int count() const { return count_; }

  constexpr bool running() const { return running_; }

  constexpr bool Tap(const Layout& layout, int x, int y) {
    if (!running_) {
      return false;
    }
    if (layout.increment.Contains(x, y)) {
      if (count_ < 9999) {
        ++count_;
      }
      return true;
    }
    if (layout.reset.Contains(x, y)) {
      count_ = 0;
      return true;
    }
    if (layout.exit.Contains(x, y)) {
      running_ = false;
      return true;
    }
    return false;
  }

 private:
  int count_ = 0;
  bool running_ = true;
};

}  // namespace gui_app

#endif  // SYMBIAN_GUI_APP_MODEL_H_
