// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "sdl2_app/assets/text.h"

#include "sdl2_app/assets/glyphs.h"
#include "sdl2_app/assets/primitives.h"

namespace arkanoid::art {

void DrawText(SDL_Renderer* absl_nonnull renderer, std::string_view message,
              int x, int y, int scale) {
  for (const char letter : message) {
    const arkanoid::art::GlyphRows& glyph = arkanoid::art::GlyphFor(letter);
    for (int row = 0; row < 7; ++row) {
      for (int column = 0; column < 5; ++column) {
        if (glyph[row] & (1 << (4 - column))) {
          arkanoid::art::FillRect(renderer, x + column * scale, y + row * scale,
                                  scale, scale);
        }
      }
    }
    x += 6 * scale;
  }
}

}  // namespace arkanoid::art
