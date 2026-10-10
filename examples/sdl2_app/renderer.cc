// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "sdl2_app/renderer.h"

#include <algorithm>
#include <string_view>

#include "sdl2_app/assets/ball.h"
#include "sdl2_app/assets/block.h"
#include "sdl2_app/assets/platform.h"
#include "sdl2_app/assets/primitives.h"
#include "sdl2_app/assets/text.h"
#include "sdl2_app/game.h"

namespace arkanoid {

void GameRenderer::Draw(const Game& game, SDL_Renderer* absl_nonnull renderer,
                        std::uint32_t frames_per_second, bool gpu_active,
                        std::string_view gpu_fallback_reason,
                        std::string_view vibration_error) {
  if (!atlas_attempted_) {
    atlas_attempted_ = true;
    block_atlas_.Open(renderer, game.cell_width_, game.brick_height_);
  }
  arkanoid::art::SetColor(renderer, 161, 208, 221);
  SDL_RenderClear(renderer);
  arkanoid::art::SetColor(renderer, 15, 81, 151);
  arkanoid::art::FillRect(renderer, 0, game.layout_.hud_top(), game.width_,
                          game.layout_.hud_height());
  for (int x = 0, column = 0; x < game.width_;
       x += game.cell_width_, ++column) {
    if (const int width = std::min(game.cell_width_, game.width_ - x);
        !block_atlas_.Draw(renderer, x, 0, width, game.brick_height_, column)) {
      arkanoid::art::DrawBlock(renderer, x, 0, width, game.brick_height_,
                               column);
    }
  }
  if (game.touchscreen_) {
    // The visible control is compact; its larger hit area stays invisible.
    const int button_width = game.layout_.pause_visual_width();
    const int button_height = game.layout_.pause_visual_height();
    arkanoid::art::SetColor(renderer, 11, 70, 126);
    arkanoid::art::FillRect(renderer, 0, 0, button_width, button_height);
    arkanoid::art::SetColor(renderer, 67, 127, 185);
    arkanoid::art::FillRect(renderer, 2, 2, button_width - 4,
                            button_height - 4);
    arkanoid::art::SetColor(renderer, 242, 248, 255);
    const int bar_width = std::max(3, button_width / 9);
    const int bar_height = std::max(10, button_height - 10);
    const int bar_left = (button_width - 3 * bar_width) / 2;
    arkanoid::art::FillRect(renderer, bar_left, 5, bar_width, bar_height);
    arkanoid::art::FillRect(renderer, bar_left + 2 * bar_width, 5, bar_width,
                            bar_height);
  }
  for (int row = 0; row < Game::kRows; ++row) {
    for (int col = 0; col < Game::kColumns; ++col) {
      if (!game.bricks_[row * Game::kColumns + col]) {
        continue;
      }
      const int x = 12 + col * game.cell_width_;
      const int y = game.layout_.brick_top() + row * game.brick_height_;
      if (const int variation = row * Game::kColumns + col;
          !block_atlas_.Draw(renderer, x, y, game.cell_width_,
                             game.brick_height_, variation)) {
        arkanoid::art::DrawBlock(renderer, x, y, game.cell_width_,
                                 game.brick_height_, variation);
      }
    }
  }
  arkanoid::art::DrawPlatform(
      renderer, static_cast<int>(game.paddle_x_ - game.paddle_width_ / 2),
      game.layout_.paddle_y(), game.paddle_width_, 12);
  arkanoid::art::DrawBall(renderer, static_cast<int>(game.ball_x_ - 7),
                          static_cast<int>(game.ball_y_ - 7), 2);
  const int life_scale = game.layout_.hud_height() >= 40 ? 3 : 2;
  for (int i = 0; i < game.lives_; ++i) {
    arkanoid::art::DrawBall(
        renderer, 12 + i * (7 * life_scale + 10),
        game.layout_.hud_top() +
            (game.layout_.hud_height() - 7 * life_scale) / 2,
        life_scale);
  }
  char digits[5] = {'0', '0', '0', '0', '0'};
  int value = game.score_ * 10;
  for (int i = 4; i >= 0; --i) {
    digits[i] = Game::kDigits[value % 10];
    value /= 10;
  }
  arkanoid::art::SetColor(renderer, 247, 249, 255);
  const int score_scale = game.layout_.hud_height() >= 40 ? 4 : 3;
  arkanoid::art::DrawText(renderer, std::string_view(digits, 5),
                          game.width_ - 5 * 6 * score_scale - 12,
                          game.layout_.hud_top() +
                              (game.layout_.hud_height() - 7 * score_scale) / 2,
                          score_scale);
  char frame_rate[] = "000 FPS";
  const std::uint32_t shown_rate =
      std::min<std::uint32_t>(frames_per_second, 999);
  frame_rate[0] = Game::kDigits[(shown_rate / 100) % 10];
  frame_rate[1] = Game::kDigits[(shown_rate / 10) % 10];
  frame_rate[2] = Game::kDigits[shown_rate % 10];
  arkanoid::art::SetColor(renderer, 11, 70, 126);
  arkanoid::art::FillRect(renderer, game.width_ - (gpu_active ? 73 : 47), 2,
                          gpu_active ? 72 : 46, 10);
  arkanoid::art::SetColor(renderer, 247, 249, 255);
  arkanoid::art::DrawText(renderer, frame_rate,
                          game.width_ - (gpu_active ? 71 : 45), 3, 1);
  if (gpu_active) {
    arkanoid::art::DrawText(renderer, "GPU", game.width_ - 24, 3, 1);
  }
  if (game.phase_ == Phase::kPaused) {
    GameRenderer::DrawPauseMenu(game, renderer, gpu_fallback_reason,
                                vibration_error);
  } else if (game.phase_ != Phase::kPlaying) {
    arkanoid::art::SetColor(renderer, 11, 70, 126);
    arkanoid::art::FillRect(renderer, 12, game.height_ / 2 - 32,
                            game.width_ - 24, 66);
    arkanoid::art::SetColor(renderer, 250, 251, 255);
    std::string_view title =
        game.touchscreen_ ? "TAP TO PLAY" : "PRESS TO PLAY";
    if (game.phase_ == Phase::kLevelClear) {
      title = "LEVEL CLEAR!";
    }
    if (game.phase_ == Phase::kWon) {
      title = "YOU WIN!";
    }
    if (game.phase_ == Phase::kLost) {
      title = "GAME OVER!";
    }
    arkanoid::art::DrawText(
        renderer, title,
        std::max(12, (game.width_ - static_cast<int>(title.size()) * 18) / 2),
        game.height_ / 2 - 20, 3);
    arkanoid::art::SetColor(renderer, 220, 235, 243);
    const std::string_view prompt =
        game.touchscreen_ ? "TAP OR SPACE" : "PRESS SELECT";
    arkanoid::art::DrawText(
        renderer, prompt,
        std::max(12, (game.width_ - static_cast<int>(prompt.size()) * 12) / 2),
        game.height_ / 2 + 14, 2);
  }
  SDL_RenderPresent(renderer);
}

void GameRenderer::DrawPauseMenu(const Game& game,
                                 SDL_Renderer* absl_nonnull renderer,
                                 std::string_view gpu_fallback_reason,
                                 std::string_view vibration_error) {
  const int top = game.MenuTop();
  const int width = game.MenuWidth();
  const int left = (game.width_ - width) / 2;
  arkanoid::art::SetColor(renderer, 11, 70, 126);
  arkanoid::art::FillRect(renderer, left, top, width, 184);
  arkanoid::art::SetColor(renderer, 250, 251, 255);
  arkanoid::art::DrawText(renderer, "PAUSED", (game.width_ - 72) / 2, top + 9,
                          2);
  if (!vibration_error.empty()) {
    const std::size_t visible = std::min<std::size_t>(
        vibration_error.size(), static_cast<std::size_t>((width - 58) / 6));
    arkanoid::art::SetColor(renderer, 183, 215, 232);
    arkanoid::art::DrawText(renderer, "VIBRA: ", left + 8, top + 26, 1);
    arkanoid::art::DrawText(renderer, vibration_error.substr(0, visible),
                            left + 50, top + 26, 1);
  }
  for (int choice = 0; choice < 3; ++choice) {
    const int y = top + 34 + choice * 47;
    if (choice == game.menu_selection_) {
      arkanoid::art::SetColor(renderer, 242, 72, 28);
    } else {
      arkanoid::art::SetColor(renderer, 67, 127, 185);
    }
    arkanoid::art::FillRect(renderer, left + 8, y, width - 16, 40);
    arkanoid::art::SetColor(renderer, 250, 251, 255);
    const std::string_view label =
        choice == 0   ? "RESUME"
        : choice == 1 ? (game.sound_enabled_ ? "SOUND ON" : "SOUND OFF")
                      : "EXIT";
    arkanoid::art::DrawText(
        renderer, label,
        (game.width_ - static_cast<int>(label.size()) * 18) / 2, y + 7, 3);
  }
  if (!gpu_fallback_reason.empty()) {
    const std::size_t visible = std::min<std::size_t>(
        gpu_fallback_reason.size(), static_cast<std::size_t>((width - 46) / 6));
    arkanoid::art::SetColor(renderer, 183, 215, 232);
    arkanoid::art::DrawText(renderer, "GPU: ", left + 8, top + 175, 1);
    arkanoid::art::DrawText(renderer, gpu_fallback_reason.substr(0, visible),
                            left + 38, top + 175, 1);
  }
}

}  // namespace arkanoid
