// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "symbian/api/time/sleep.h"

#include <chrono>
#include <cstdint>

#include "abi.h"

namespace symbian::api::time {

void SleepFor(std::chrono::nanoseconds duration) {
  if (duration <= std::chrono::nanoseconds::zero()) {
    return;
  }
  const std::int64_t nanoseconds = duration.count();
  const std::uint64_t microseconds = static_cast<std::uint64_t>(
      nanoseconds / 1000 + (nanoseconds % 1000 != 0 ? 1 : 0));
  (void)SymbianRuntimeSleepMicroseconds(microseconds);
}

}  // namespace symbian::api::time
