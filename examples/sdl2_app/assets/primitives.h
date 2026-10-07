// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_EXAMPLES_SDL2_APP_ASSETS_PRIMITIVES_H_
#define SYMBIAN_EXAMPLES_SDL2_APP_ASSETS_PRIMITIVES_H_

#include <cstdint>

#include "sdl2_app/arkanoid_adapter.h"

namespace arkanoid::art {

void SetColor(SDL_Renderer* absl_nonnull renderer, std::uint8_t red,
              std::uint8_t green, std::uint8_t blue);
void FillRect(SDL_Renderer* absl_nonnull renderer, int x, int y, int width,
              int height);

}  // namespace arkanoid::art

#endif  // SYMBIAN_EXAMPLES_SDL2_APP_ASSETS_PRIMITIVES_H_
