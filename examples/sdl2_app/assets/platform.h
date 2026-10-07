// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_EXAMPLES_SDL2_APP_ASSETS_PLATFORM_H_
#define SYMBIAN_EXAMPLES_SDL2_APP_ASSETS_PLATFORM_H_

#include "sdl2_app/arkanoid_adapter.h"

namespace arkanoid::art {

void DrawPlatform(SDL_Renderer* absl_nonnull renderer, int x, int y, int width,
                  int height);

}  // namespace arkanoid::art

#endif  // SYMBIAN_EXAMPLES_SDL2_APP_ASSETS_PLATFORM_H_
