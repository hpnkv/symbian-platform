// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_API_MEDIA_GAME_FEEDBACK_H_
#define SYMBIAN_API_MEDIA_GAME_FEEDBACK_H_

#include <cstdint>

#include "absl/base/nullability.h"
#include "absl/status/status.h"

namespace symbian::api::media {

/**
 * @brief Optional MIDI melody and light tactile feedback for a foreground game.
 *
 * A missing service leaves gameplay intact. Public methods run on the opening
 * thread. Hit dispatches the synchronous native touch/HWRM request to a
 * worker; the tactile path uses an already running system server, avoiding
 * optional haptics-stack startup during play. Pump processes ready callbacks
 * without blocking the frame.
 */
class GameFeedback final {
 public:
  GameFeedback();
  GameFeedback(const GameFeedback&) = delete;
  GameFeedback& operator=(const GameFeedback&) = delete;
  ~GameFeedback();

  void Start();
  void SetMusicEnabled(bool enabled);
  void Pump(std::uint64_t elapsed_ms);
  void Hit(std::uint64_t elapsed_ms);
  bool midi_available() const;
  bool vibration_available() const;
  absl::Status vibration_status() const;

 private:
  struct Impl;
  Impl* absl_nullable impl_ = nullptr;
};

}  // namespace symbian::api::media

#endif  // SYMBIAN_API_MEDIA_GAME_FEEDBACK_H_
