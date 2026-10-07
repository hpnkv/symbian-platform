// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "sdl2_app/assets/primitives.h"

namespace arkanoid::art {

void SetColor(SDL_Renderer* absl_nonnull renderer, std::uint8_t red,
              std::uint8_t green, std::uint8_t blue) {
  SDL_SetRenderDrawColor(renderer, red, green, blue, 255);
}

void FillRect(SDL_Renderer* absl_nonnull renderer, int x, int y, int width,
              int height) {
  arkanoid::Fill(renderer, x, y, width, height);
}

}  // namespace arkanoid::art
