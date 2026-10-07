// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_EXAMPLES_SDL2_APP_ASSETS_TEXT_H_
#define SYMBIAN_EXAMPLES_SDL2_APP_ASSETS_TEXT_H_

#include <string_view>

#include "sdl2_app/arkanoid_adapter.h"

namespace arkanoid::art {

void DrawText(SDL_Renderer* absl_nonnull renderer, std::string_view message,
              int x, int y, int scale);

}  // namespace arkanoid::art

#endif  // SYMBIAN_EXAMPLES_SDL2_APP_ASSETS_TEXT_H_
