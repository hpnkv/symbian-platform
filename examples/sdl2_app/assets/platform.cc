// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "sdl2_app/assets/platform.h"

#include "sdl2_app/assets/primitives.h"

namespace arkanoid::art {

void DrawPlatform(SDL_Renderer* absl_nonnull renderer, int x, int y, int width,
                  int height) {
  arkanoid::art::SetColor(renderer, 66, 79, 86);
  arkanoid::art::FillRect(renderer, x, y, width, height);
  arkanoid::art::SetColor(renderer, 193, 209, 215);
  arkanoid::art::FillRect(renderer, x + 3, y + 1, width - 6, height - 5);
}

}  // namespace arkanoid::art
