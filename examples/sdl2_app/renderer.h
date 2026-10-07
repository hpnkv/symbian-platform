// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_EXAMPLES_SDL2_APP_RENDERER_H_
#define SYMBIAN_EXAMPLES_SDL2_APP_RENDERER_H_

#include <string_view>

#include "sdl2_app/arkanoid_adapter.h"
#include "sdl2_app/assets/block_atlas.h"

namespace arkanoid {

class Game;

class GameRenderer final {
 public:
  void Draw(const Game& game, SDL_Renderer* absl_nonnull renderer,
            std::uint32_t frames_per_second, bool gpu_active,
            std::string_view gpu_fallback_reason,
            std::string_view vibration_error);

 private:
  static void DrawPauseMenu(const Game& game,
                            SDL_Renderer* absl_nonnull renderer,
                            std::string_view gpu_fallback_reason,
                            std::string_view vibration_error);
  arkanoid::art::BlockAtlas block_atlas_;
  bool atlas_attempted_ = false;
};

}  // namespace arkanoid

#endif  // SYMBIAN_EXAMPLES_SDL2_APP_RENDERER_H_
