// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "sdl2_app/assets/block.h"

#include "sdl2_app/assets/primitives.h"

namespace arkanoid::art {

void DrawBlock(SDL_Renderer* absl_nonnull renderer, int x, int y, int width,
               int height, int variation) {
  arkanoid::art::SetColor(renderer, 79, 20, 18);
  arkanoid::art::FillRect(renderer, x, y, width, height);
  arkanoid::art::SetColor(renderer, 207, 49, 19);
  arkanoid::art::FillRect(renderer, x + 1, y + 1, width - 2, height - 3);
  arkanoid::art::SetColor(renderer, 240, 68, 23);
  arkanoid::art::FillRect(renderer, x + 2, y + 2, width - 4, 3);
  arkanoid::art::SetColor(renderer, 159, 37, 20);
  arkanoid::art::FillRect(renderer, x + 2, y + height / 2, width - 4, 2);
  arkanoid::art::SetColor(renderer, 128, 30, 21);
  arkanoid::art::FillRect(renderer, x + (variation * 7 + 3) % (width - 7),
                          y + 5, 4, 2);
  arkanoid::art::FillRect(renderer, x + (variation * 5 + 9) % (width - 8),
                          y + height - 6, 5, 2);
  arkanoid::art::SetColor(renderer, 107, 24, 21);
  arkanoid::art::FillRect(renderer, x + 2, y + height - 3, width - 4, 2);
}

}  // namespace arkanoid::art
