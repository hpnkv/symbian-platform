// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_API_MEDIA_VIBRATION_H_
#define SYMBIAN_API_MEDIA_VIBRATION_H_

#include <memory>

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"

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
  static absl::StatusOr<Vibration> Create();
  static absl::StatusOr<std::unique_ptr<Vibration>> CreateUnique();
  Vibration(const Vibration&) = delete;
  Vibration& operator=(const Vibration&) = delete;
  Vibration(Vibration&& other) noexcept;
  Vibration& operator=(Vibration&& other) noexcept;
  ~Vibration();

  absl::Status Start();
  absl::Status Pulse(int duration_ms);
  absl::Status status();

 private:
  struct Impl;
  explicit Vibration(std::unique_ptr<Impl> impl);
  std::unique_ptr<Impl> impl_;
};

}  // namespace symbian::api::media

#endif  // SYMBIAN_API_MEDIA_VIBRATION_H_
