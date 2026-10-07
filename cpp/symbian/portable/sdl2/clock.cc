// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include <cstdint>

#include <e32std.h>

#include "SDL.h"
#include "symbian/api/time/monotonic_clock.h"

namespace {
std::int64_t start_nanoseconds = 0;
}

extern "C" void SDL_TicksInit(void) {
  start_nanoseconds = symbian::api::time::MonotonicClock::NowNanoseconds();
}

extern "C" void SDL_TicksQuit(void) {}

extern "C" Uint64 SDL_GetTicks64(void) {
  if (start_nanoseconds == 0) SDL_TicksInit();
  return static_cast<std::uint64_t>(
      (symbian::api::time::MonotonicClock::NowNanoseconds() -
       start_nanoseconds) /
      1000000);
}

extern "C" Uint64 SDL_GetPerformanceCounter(void) {
  return static_cast<std::uint64_t>(
      symbian::api::time::MonotonicClock::NowNanoseconds());
}

extern "C" Uint64 SDL_GetPerformanceFrequency(void) { return 1000000000ULL; }

extern "C" void SDL_Delay(Uint32 ms) {
  while (ms > 0) {
    const std::uint32_t slice = ms > 1000 ? 1000 : ms;
    User::After(static_cast<TInt>(slice * 1000));
    ms -= slice;
  }
}
