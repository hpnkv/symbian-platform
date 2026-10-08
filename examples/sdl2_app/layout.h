// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_EXAMPLES_SDL2_APP_LAYOUT_H_
#define SYMBIAN_EXAMPLES_SDL2_APP_LAYOUT_H_

namespace arkanoid {

class GameLayout final {
 public:
  GameLayout(int width, int height);

  int hud_height() const { return hud_height_; }
  int hud_top() const { return height_ - hud_height_; }
  int paddle_y() const { return paddle_y_; }
  int ready_ball_y() const { return paddle_y_ - 12; }
  int miss_y() const { return hud_top() - 4; }
  int brick_top() const { return brick_top_; }
  int pause_visual_width() const { return pause_visual_width_; }
  int pause_visual_height() const { return pause_visual_height_; }
  int pause_hit_size() const { return pause_hit_size_; }

 private:
  int height_;
  int hud_height_;
  int paddle_y_;
  int brick_top_;
  int pause_visual_width_;
  int pause_visual_height_;
  int pause_hit_size_;
};

}  // namespace arkanoid

#endif  // SYMBIAN_EXAMPLES_SDL2_APP_LAYOUT_H_
