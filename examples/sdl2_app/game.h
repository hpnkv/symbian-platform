// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_EXAMPLES_SDL2_APP_GAME_H_
#define SYMBIAN_EXAMPLES_SDL2_APP_GAME_H_

#include <array>
#include <cstdint>

#include "sdl2_app/arkanoid_adapter.h"
#include "sdl2_app/layout.h"

namespace arkanoid {

enum class Phase { kReady, kPlaying, kPaused, kLevelClear, kWon, kLost };

class Game final {
 public:
  Game(int width, int height, bool touchscreen);

  void Handle(const SDL_Event& event);
  void Step();

  std::uint32_t hits() const;
  bool exit_requested() const;
  bool music_enabled() const;

 private:
  friend class GameRenderer;
  static constexpr int kColumns = 10;
  static constexpr int kRows = 6;
  static constexpr int kLevels = 3;

  int MenuTop() const;
  int MenuWidth() const;
  void SelectMenuAt(int x, int y, bool activate);
  void ActivateMenuSelection();
  void Pause();
  void Resume();
  void Activate();
  void ResetLevel();
  void FollowPointer();

  static constexpr char kDigits[] = "0123456789";
  const int width_;
  const int height_;
  const bool touchscreen_;
  const GameLayout layout_;
  const int cell_width_;
  const int brick_height_;
  std::array<bool, kRows * kColumns> bricks_{};
  Phase phase_ = Phase::kReady;
  Phase resume_phase_ = Phase::kReady;
  int menu_selection_ = 0;
  bool exit_requested_ = false;
  bool sound_enabled_ = true;
  int level_ = 1;
  int lives_ = 3;
  int score_ = 0;
  int remaining_ = 0;
  int pointer_x_ = 0;
  int paddle_width_ = 64;
  float paddle_x_ = 0;
  float ball_x_ = 0;
  float ball_y_ = 0;
  float velocity_x_ = 0;
  float velocity_y_ = 0;
  bool left_ = false;
  bool right_ = false;
  bool pointer_active_ = false;
  std::uint32_t hits_ = 0;
};

}  // namespace arkanoid

#endif  // SYMBIAN_EXAMPLES_SDL2_APP_GAME_H_
