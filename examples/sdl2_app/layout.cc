// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "sdl2_app/layout.h"

#include <algorithm>

namespace arkanoid {

GameLayout::GameLayout(int width, int height)
    : height_(height),
      hud_height_(std::clamp(height / 12, 34, 46)),
      paddle_y_(height - hud_height_ - std::clamp(height / 24, 14, 24)),
      brick_top_(std::clamp(height / 10, 42, 64)),
      pause_visual_width_(std::clamp(width / 12, 28, 36)),
      pause_visual_height_(std::clamp(height / 24, 24, 32)),
      pause_hit_size_(std::clamp(std::min(width, height) / 5, 44, 56)) {}

}  // namespace arkanoid
