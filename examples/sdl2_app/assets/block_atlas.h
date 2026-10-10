// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_EXAMPLES_SDL2_APP_ASSETS_BLOCK_ATLAS_H_
#define SYMBIAN_EXAMPLES_SDL2_APP_ASSETS_BLOCK_ATLAS_H_

#include <memory>

#include <absl/status/status.h>
#include <absl/status/statusor.h>

#include "sdl2_app/arkanoid_adapter.h"

namespace arkanoid::art {

// One small immutable texture holds all six rows of ten block variations.
// The renderer must outlive this atlas.
class BlockAtlas final {
 public:
  static absl::StatusOr<BlockAtlas> Create(SDL_Renderer* absl_nonnull renderer,
                                           int cell_width, int cell_height);
  static absl::StatusOr<std::unique_ptr<BlockAtlas>> CreateUnique(
      SDL_Renderer* absl_nonnull renderer, int cell_width, int cell_height);
  BlockAtlas(const BlockAtlas&) = delete;
  BlockAtlas& operator=(const BlockAtlas&) = delete;
  BlockAtlas(BlockAtlas&& other) noexcept;
  BlockAtlas& operator=(BlockAtlas&& other) noexcept;
  ~BlockAtlas();

  bool Draw(SDL_Renderer* absl_nonnull renderer, int x, int y, int width,
            int height, int variation) const;

 private:
  BlockAtlas() = default;
  absl::Status Open(SDL_Renderer* absl_nonnull renderer, int cell_width,
                    int cell_height);
  SDL_Texture* absl_nullable texture_ = nullptr;
  int cell_width_ = 0;
  int cell_height_ = 0;
};

}  // namespace arkanoid::art

#endif  // SYMBIAN_EXAMPLES_SDL2_APP_ASSETS_BLOCK_ATLAS_H_
