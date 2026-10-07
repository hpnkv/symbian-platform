// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_EXAMPLES_SDL2_APP_ASSETS_GLYPHS_H_
#define SYMBIAN_EXAMPLES_SDL2_APP_ASSETS_GLYPHS_H_

#include <array>
#include <cstdint>

namespace arkanoid::art {

using GlyphRows = std::array<std::uint8_t, 7>;

// Covers every printable ASCII character, from space through tilde.
const GlyphRows& GlyphFor(char letter);

}  // namespace arkanoid::art

#endif  // SYMBIAN_EXAMPLES_SDL2_APP_ASSETS_GLYPHS_H_
