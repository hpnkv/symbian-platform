// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_API_MEDIA_VIBRATION_H_
#define SYMBIAN_API_MEDIA_VIBRATION_H_

#include "absl/base/nullability.h"
#include "absl/status/status.h"

namespace symbian::api::media {

/**
 * @brief Optional HWRM vibration pulses on a dedicated service thread.
 *
 * Start prepares the worker before latency-sensitive use. Pulse admits one
 * request at a time and returns immediately; status reports its later native
 * result or a stalled-service deadline. Duration uses milliseconds and the
 * device's default intensity. The caller chooses when to request a pulse.
 */
class Vibration final {
 public:
  Vibration();
  Vibration(const Vibration&) = delete;
  Vibration& operator=(const Vibration&) = delete;
  ~Vibration();

  absl::Status Start();
  absl::Status Pulse(int duration_ms);
  absl::Status status();

 private:
  struct Impl;
  Impl* absl_nullable impl_ = nullptr;
};

}  // namespace symbian::api::media

#endif  // SYMBIAN_API_MEDIA_VIBRATION_H_
