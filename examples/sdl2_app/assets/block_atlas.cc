// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "sdl2_app/assets/block_atlas.h"

#include <cstdint>
#include <memory>
#include <new>

namespace arkanoid::art {
namespace {

constexpr int kColumns = 10;
constexpr int kRows = 6;

constexpr std::uint32_t Argb(std::uint32_t red, std::uint32_t green,
                             std::uint32_t blue) {
  return 0xff000000u | (red << 16) | (green << 8) | blue;
}

void FillPixels(std::uint32_t* absl_nonnull pixels, int pitch, int x, int y,
                int width, int height, std::uint32_t color) {
  for (int row = y; row < y + height; ++row) {
    for (int column = x; column < x + width; ++column) {
      pixels[row * pitch + column] = color;
    }
  }
}

void PaintBlock(std::uint32_t* absl_nonnull pixels, int pitch, int x, int y,
                int width, int height, int variation) {
  FillPixels(pixels, pitch, x, y, width, height, Argb(79, 20, 18));
  FillPixels(pixels, pitch, x + 1, y + 1, width - 2, height - 3,
             Argb(207, 49, 19));
  FillPixels(pixels, pitch, x + 2, y + 2, width - 4, 3, Argb(240, 68, 23));
  FillPixels(pixels, pitch, x + 2, y + height / 2, width - 4, 2,
             Argb(159, 37, 20));
  FillPixels(pixels, pitch, x + (variation * 7 + 3) % (width - 7), y + 5, 4, 2,
             Argb(128, 30, 21));
  FillPixels(pixels, pitch, x + (variation * 5 + 9) % (width - 8),
             y + height - 6, 5, 2, Argb(128, 30, 21));
  FillPixels(pixels, pitch, x + 2, y + height - 3, width - 4, 2,
             Argb(107, 24, 21));
}

}  // namespace

BlockAtlas::~BlockAtlas() {
  if (texture_ != nullptr) {
    SDL_DestroyTexture(texture_);
  }
}

bool BlockAtlas::Open(SDL_Renderer* absl_nonnull renderer, int cell_width,
                      int cell_height) {
  if (texture_ != nullptr || cell_width < 9 || cell_height < 9) {
    return false;
  }
  const int width = kColumns * cell_width;
  const int height = kRows * cell_height;
  const std::size_t count = static_cast<std::size_t>(width) * height;
  std::unique_ptr<std::uint32_t[]> pixels(new (std::nothrow)
                                              std::uint32_t[count]);
  if (pixels == nullptr) {
    return false;
  }
  for (int row = 0; row < kRows; ++row) {
    for (int column = 0; column < kColumns; ++column) {
      PaintBlock(pixels.get(), width, column * cell_width, row * cell_height,
                 cell_width, cell_height, row * kColumns + column);
    }
  }
  texture_ = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                               SDL_TEXTUREACCESS_STATIC, width, height);
  if (texture_ == nullptr) {
    return false;
  }
  if (!arkanoid::UpdateTexture(texture_, pixels.get(), width * 4)) {
    SDL_DestroyTexture(texture_);
    texture_ = nullptr;
    return false;
  }
  SDL_SetTextureBlendMode(texture_, SDL_BLENDMODE_NONE);
  cell_width_ = cell_width;
  cell_height_ = cell_height;
  return true;
}

bool BlockAtlas::Draw(SDL_Renderer* absl_nonnull renderer, int x, int y,
                      int width, int height, int variation) const {
  if (texture_ == nullptr || width <= 0 || width > cell_width_ ||
      height != cell_height_ || variation < 0 ||
      variation >= kColumns * kRows) {
    return false;
  }
  arkanoid::DrawTexture(
      renderer, texture_, (variation % kColumns) * cell_width_,
      (variation / kColumns) * cell_height_, width, height, x, y);
  return true;
}

}  // namespace arkanoid::art
