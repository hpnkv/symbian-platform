// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include <cstdint>

#include <e32std.h>

#include <SDL3/SDL.h>

#include "symbian/api/time/monotonic_clock.h"

extern "C" Uint64 SDL_GetPerformanceCounter(void) {
  return static_cast<std::uint64_t>(
      symbian::api::time::MonotonicClock::NowNanoseconds());
}

extern "C" Uint64 SDL_GetPerformanceFrequency(void) { return 1000000000ULL; }

extern "C" void SDL_SYS_DelayNS(Uint64 ns) {
  std::uint64_t microseconds = (ns + 999) / 1000;
  while (microseconds != 0) {
    const std::uint64_t slice =
        microseconds > 1000000 ? 1000000 : microseconds;
    User::After(static_cast<TInt>(slice));
    microseconds -= slice;
  }
}
