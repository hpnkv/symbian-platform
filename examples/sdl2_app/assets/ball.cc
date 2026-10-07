// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "sdl2_app/assets/ball.h"

#include <cstdint>

#include "sdl2_app/assets/primitives.h"

namespace arkanoid::art {

void DrawBall(SDL_Renderer* absl_nonnull renderer, int x, int y, int scale) {
  constexpr std::uint8_t kShape[7] = {0b0011100, 0b0111110, 0b1111111,
                                      0b1111111, 0b1111111, 0b0111110,
                                      0b0011100};
  for (int row = 0; row < 7; ++row) {
    for (int column = 0; column < 7; ++column) {
      if (!(kShape[row] & (1 << (6 - column)))) {
        continue;
      }
      if (row >= 5 || column == 6) {
        arkanoid::art::SetColor(renderer, 119, 17, 15);
      } else if (row <= 2 && column >= 2 && column <= 4) {
        arkanoid::art::SetColor(renderer, 255, 116, 72);
      } else {
        arkanoid::art::SetColor(renderer, 219, 32, 20);
      }
      arkanoid::art::FillRect(renderer, x + column * scale, y + row * scale,
                              scale, scale);
    }
  }
  arkanoid::art::SetColor(renderer, 255, 211, 172);
  arkanoid::art::FillRect(renderer, x + 2 * scale, y + scale, scale, scale);
}

}  // namespace arkanoid::art
