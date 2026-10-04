// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_API_SYSTEM_COUNTERS_H_
#define SYMBIAN_API_SYSTEM_COUNTERS_H_

#include <cstdint>

#include "absl/status/statusor.h"
#include "absl/time/time.h"

namespace symbian::api::system {

/**
 * @brief Native 32-bit elapsed-time tick and its measured period.
 *
 * The count wraps. Subtract nearby readings with unsigned arithmetic to
 * include one wrap; this is not a wall clock or a long-term uptime value.
 */
struct TickReading {
  /** @brief Native tick count sampled after the period lookup. */
  std::uint32_t count = 0;
  /** @brief Duration of one tick reported by the current platform. */
  absl::Duration period = absl::ZeroDuration();
};

/** @brief Native 32-bit fast counter and platform-reported frequency. */
struct FastCounterReading {
  /** @brief Counter value sampled after the frequency lookup. */
  std::uint32_t count = 0;
  /** @brief Native ticks per second; do not assume a fixed device value. */
  std::uint32_t ticks_per_second = 0;
};

/** @brief Read the native system tick and its period, or a typed OS error. */
absl::StatusOr<TickReading> ReadTickCounter();

/**
 * @brief Read the fast counter and its frequency, or a typed OS error.
 *
 * Availability and resolution depend on the device or emulator profile.
 */
absl::StatusOr<FastCounterReading> ReadFastCounter();

}  // namespace symbian::api::system

#endif  // SYMBIAN_API_SYSTEM_COUNTERS_H_
