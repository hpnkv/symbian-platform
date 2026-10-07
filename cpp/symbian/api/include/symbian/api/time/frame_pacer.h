// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_API_TIME_FRAME_PACER_H_
#define SYMBIAN_API_TIME_FRAME_PACER_H_

#include <algorithm>
#include <cstdint>

#include "symbian/api/time/monotonic_clock.h"

namespace symbian::api::time {

/** @brief Monotonic frame deadlines for a reported display refresh rate. */
class FramePacer final {
 public:
  explicit FramePacer(std::uint32_t refresh_rate_hz)
      : period_ns_(
            (1000000000LL + std::max<std::uint32_t>(refresh_rate_hz, 1) - 1) /
            std::max<std::uint32_t>(refresh_rate_hz, 1)) {
    Reset();
  }

  void Reset() { deadline_ns_ = MonotonicClock::NowNanoseconds() + period_ns_; }

  /**
   * @brief Return a whole millisecond sleep toward the next frame deadline.
   *
   * Call once after each presentation. A missed deadline returns immediately
   * and starts a new period instead of adding another full-period wait.
   */
  std::uint32_t NextDelayMilliseconds() {
    const std::int64_t now = MonotonicClock::NowNanoseconds();
    if (now >= deadline_ns_) {
      deadline_ns_ = now + period_ns_;
      return 0;
    }
    const std::int64_t remaining = deadline_ns_ - now;
    deadline_ns_ += period_ns_;
    return static_cast<std::uint32_t>((remaining + 999999) / 1000000);
  }

 private:
  std::int64_t period_ns_;
  std::int64_t deadline_ns_ = 0;
};

}  // namespace symbian::api::time

#endif  // SYMBIAN_API_TIME_FRAME_PACER_H_
