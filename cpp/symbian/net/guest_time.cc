// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.
// nghttp2 rate limiting needs monotonic seconds. Use the runtime's extended
// native tick counter rather than Open C's unverified CLOCK_MONOTONIC path.
#include <chrono>
#include <cstdint>

extern "C" std::uint64_t nghttp2_time_now_sec() {
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::seconds>(
          std::chrono::steady_clock::now().time_since_epoch())
          .count());
}
