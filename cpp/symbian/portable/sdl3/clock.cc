// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <limits>

#include <SDL3/SDL.h>

#include "symbian/api/time/monotonic_clock.h"
#include "symbian/api/time/sleep.h"

extern "C" Uint64 SDL_GetPerformanceCounter(void) {
  return static_cast<std::uint64_t>(
      symbian::api::time::MonotonicClock::NowNanoseconds());
}

extern "C" Uint64 SDL_GetPerformanceFrequency(void) { return 1000000000ULL; }

extern "C" void SDL_SYS_DelayNS(Uint64 ns) {
  const std::uint64_t bounded = std::min<std::uint64_t>(
      ns, static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()));
  symbian::api::time::SleepFor(
      std::chrono::nanoseconds(static_cast<std::int64_t>(bounded)));
}
