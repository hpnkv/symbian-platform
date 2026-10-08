// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "sdl2_app/game.h"

#include <algorithm>

namespace arkanoid {

Game::Game(int width, int height, bool touchscreen)
    : width_(width),
      height_(height),
      touchscreen_(touchscreen),
      layout_(width, height),
      cell_width_((width - 24) / kColumns),
      brick_height_(std::max(13, height / 24)) {
  ResetLevel();
}

void Game::Handle(const SDL_Event& event) {
  if (arkanoid::Backgrounded(event)) {
    Pause();
    return;
  }
  if (arkanoid::KeyDown(event) || arkanoid::KeyUp(event)) {
    const bool pressed = arkanoid::KeyDown(event);
    const SDL_Keycode key = arkanoid::Key(event);
    if (pressed && key == SDLK_ESCAPE) {
      if (phase_ == Phase::kPaused) {
        Resume();
      } else {
        Pause();
      }
      return;
    }
    if (phase_ == Phase::kPaused) {
      if (pressed && (key == SDLK_UP || key == SDLK_LEFT)) {
        menu_selection_ = (menu_selection_ + 2) % 3;
      }
      if (pressed && (key == SDLK_DOWN || key == SDLK_RIGHT)) {
        menu_selection_ = (menu_selection_ + 1) % 3;
      }
      if (pressed && (key == SDLK_SPACE || key == SDLK_RETURN)) {
        ActivateMenuSelection();
      }
      return;
    }
    if (key == SDLK_LEFT) {
      left_ = pressed;
      if (pressed) {
        pointer_active_ = false;
      }
    }
    if (key == SDLK_RIGHT) {
      right_ = pressed;
      if (pressed) {
        pointer_active_ = false;
      }
    }
    if (pressed && (key == SDLK_SPACE || key == SDLK_RETURN)) {
      Activate();
    }
  }
  if (phase_ == Phase::kPaused) {
    if (arkanoid::MouseMotion(event)) {
      SelectMenuAt(arkanoid::MotionX(event), arkanoid::MotionY(event), false);
    }
    if (arkanoid::MouseDown(event)) {
      SelectMenuAt(arkanoid::ButtonX(event), arkanoid::ButtonY(event), true);
    }
    return;
  }
  if (touchscreen_ && arkanoid::MouseDown(event) &&
      arkanoid::ButtonX(event) >= 0 &&
      arkanoid::ButtonX(event) < layout_.pause_hit_size() &&
      arkanoid::ButtonY(event) >= 0 &&
      arkanoid::ButtonY(event) < layout_.pause_hit_size()) {
    Pause();
    return;
  }
  if (arkanoid::MouseMotion(event)) {
    pointer_x_ = arkanoid::MotionX(event);
    pointer_active_ = true;
    FollowPointer();
  }
  if (arkanoid::MouseDown(event)) {
    pointer_x_ = arkanoid::ButtonX(event);
    pointer_active_ = true;
    FollowPointer();
    Activate();
  }
}

void Game::Step() {
  if (phase_ == Phase::kPaused) {
    return;
  }
  const float key_speed = std::clamp(width_ * 0.011f, 4.5f, 8.0f);
  if (left_) {
    paddle_x_ -= key_speed;
  }
  if (right_) {
    paddle_x_ += key_speed;
  }
  if (pointer_active_) {
    FollowPointer();
  }
  paddle_x_ = std::clamp(paddle_x_, paddle_width_ / 2.0f,
                         width_ - paddle_width_ / 2.0f);
  if (phase_ != Phase::kPlaying) {
    if (phase_ == Phase::kReady) {
      ball_x_ = paddle_x_;
    }
    return;
  }
  const float previous_x = ball_x_;
  const float previous_y = ball_y_;
  ball_x_ += velocity_x_;
  ball_y_ += velocity_y_;
  constexpr float kBallRadius = 7.0f;
  if (ball_x_ <= kBallRadius) {
    ball_x_ = kBallRadius;
    velocity_x_ = std::max(velocity_x_, 1.0f);
  } else if (ball_x_ >= width_ - kBallRadius) {
    ball_x_ = width_ - kBallRadius;
    velocity_x_ = std::min(velocity_x_, -1.0f);
  }
  const float ceiling_y = static_cast<float>(brick_height_) + kBallRadius;
  if (ball_y_ <= ceiling_y) {
    ball_y_ = ceiling_y;
    velocity_y_ = std::max(velocity_y_, 1.0f);
  }
  const float paddle_y = static_cast<float>(layout_.paddle_y());
  const float paddle_left = paddle_x_ - paddle_width_ / 2.0f;
  const float paddle_right = paddle_x_ + paddle_width_ / 2.0f;
  const bool reaches_paddle =
      previous_y - kBallRadius <= paddle_y &&
      ball_y_ + kBallRadius >= paddle_y &&
      ball_y_ - kBallRadius <= paddle_y;
  const float travel_y = ball_y_ - previous_y;
  const float contact_fraction =
      travel_y > 0
          ? std::clamp((paddle_y - kBallRadius - previous_y) / travel_y,
                       0.0f, 1.0f)
          : 1.0f;
  const float contact_x =
      previous_x + (ball_x_ - previous_x) * contact_fraction;
  if (velocity_y_ > 0 && reaches_paddle &&
      contact_x + kBallRadius >= paddle_left &&
      contact_x - kBallRadius <= paddle_right) {
    ball_y_ = paddle_y - kBallRadius;
    velocity_y_ = -velocity_y_;
    velocity_x_ = std::clamp((ball_x_ - paddle_x_) * 0.085f, -5.0f, 5.0f);
    ++hits_;
  }
  const int top = layout_.brick_top();
  for (int row = 0; row < kRows; ++row) {
    for (int col = 0; col < kColumns; ++col) {
      if (!bricks_[row * kColumns + col]) {
        continue;
      }
      const int x = 12 + col * cell_width_;
      const int y = top + row * brick_height_;
      if (ball_x_ + 6 >= x && ball_x_ - 6 <= x + cell_width_ &&
          ball_y_ + 6 >= y && ball_y_ - 6 <= y + brick_height_) {
        bricks_[row * kColumns + col] = false;
        velocity_y_ = -velocity_y_;
        ++hits_;
        ++score_;
        if (--remaining_ == 0) {
          phase_ = level_ == kLevels ? Phase::kWon : Phase::kLevelClear;
        }
        return;
      }
    }
  }
  if (ball_y_ > layout_.miss_y()) {
    if (--lives_ == 0) {
      phase_ = Phase::kLost;
    } else {
      phase_ = Phase::kReady;
      ball_x_ = paddle_x_;
      ball_y_ = static_cast<float>(layout_.ready_ball_y());
    }
  }
}

std::uint32_t Game::hits() const {
  return hits_;
}

bool Game::exit_requested() const {
  return exit_requested_;
}

bool Game::music_enabled() const {
  return sound_enabled_ && phase_ != Phase::kPaused;
}

int Game::MenuTop() const {
  return std::max(8, (height_ - 184) / 2);
}

int Game::MenuWidth() const {
  return std::min(280, width_ - 24);
}

void Game::SelectMenuAt(int x, int y, bool activate) {
  const int left = (width_ - MenuWidth()) / 2 + 8;
  if (x < left || x >= left + MenuWidth() - 16) {
    return;
  }
  for (int choice = 0; choice < 3; ++choice) {
    const int top = MenuTop() + 34 + choice * 47;
    if (y < top || y >= top + 40) {
      continue;
    }
    menu_selection_ = choice;
    if (activate) {
      ActivateMenuSelection();
    }
    return;
  }
}

void Game::ActivateMenuSelection() {
  if (menu_selection_ == 0) {
    Resume();
  } else if (menu_selection_ == 1) {
    sound_enabled_ = !sound_enabled_;
  } else {
    exit_requested_ = true;
  }
}

void Game::Pause() {
  if (phase_ == Phase::kPaused) {
    return;
  }
  resume_phase_ = phase_;
  phase_ = Phase::kPaused;
  left_ = false;
  right_ = false;
  pointer_active_ = false;
  menu_selection_ = 0;
}

void Game::Resume() {
  if (phase_ == Phase::kPaused) {
    phase_ = resume_phase_;
  }
}

void Game::Activate() {
  if (phase_ == Phase::kWon || phase_ == Phase::kLost) {
    level_ = 1;
    lives_ = 3;
    score_ = 0;
    ResetLevel();
  } else if (phase_ == Phase::kLevelClear) {
    ++level_;
    ResetLevel();
  }
  if (phase_ == Phase::kReady && pointer_active_) {
    paddle_x_ = std::clamp(static_cast<float>(pointer_x_), paddle_width_ / 2.0f,
                           width_ - paddle_width_ / 2.0f);
    ball_x_ = paddle_x_;
  }
  phase_ = Phase::kPlaying;
}

void Game::ResetLevel() {
  remaining_ = 0;
  for (int row = 0; row < kRows; ++row) {
    for (int col = 0; col < kColumns; ++col) {
      const bool present = level_ == 1   ? row < 3
                           : level_ == 2 ? ((row + col) % 3 != 0)
                                         : (row < 2 || (col % 2 == 0));
      bricks_[row * kColumns + col] = present;
      if (present) {
        ++remaining_;
      }
    }
  }
  paddle_x_ = width_ / 2.0f;
  paddle_width_ = std::clamp(width_ / 4, 64, 96);
  ball_x_ = paddle_x_;
  ball_y_ = static_cast<float>(layout_.ready_ball_y());
  velocity_x_ = 2.7f + level_ * 0.4f;
  velocity_y_ = -(3.7f + level_ * 0.4f);
  phase_ = Phase::kReady;
}

void Game::FollowPointer() {
  paddle_x_ = std::clamp(static_cast<float>(pointer_x_), paddle_width_ / 2.0f,
                         width_ - paddle_width_ / 2.0f);
  if (phase_ == Phase::kReady) {
    ball_x_ = paddle_x_;
  }
}

}  // namespace arkanoid
