// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_API_TIME_MONOTONIC_CLOCK_H_
#define SYMBIAN_API_TIME_MONOTONIC_CLOCK_H_

#include <chrono>
#include <cstdint>

namespace symbian::api::time {

// The SDK runtime extends the native wrapping tick source for steady_clock.
class MonotonicClock final {
 public:
  static std::int64_t NowNanoseconds() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
  }
};

}  // namespace symbian::api::time

#endif  // SYMBIAN_API_TIME_MONOTONIC_CLOCK_H_
