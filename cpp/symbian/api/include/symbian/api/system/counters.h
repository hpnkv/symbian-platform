// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_API_SYSTEM_COUNTERS_H_
#define SYMBIAN_API_SYSTEM_COUNTERS_H_

#include <chrono>
#include <cstdint>

#include "absl/status/statusor.h"

namespace symbian::api::system {

// A reading of the native system tick counter. The count wraps at 32 bits;
// subtract nearby readings with unsigned arithmetic to include one wrap.
struct TickReading {
  // Native tick count sampled after the period lookup.
  std::uint32_t count = 0;
  // Duration of one tick reported by the current native platform.
  std::chrono::microseconds period{0};
};

// A reading of the native high-resolution counter. Its frequency is a device
// property, not an assumed constant. The count wraps at 32 bits.
struct FastCounterReading {
  // Native fast-counter count sampled after the frequency lookup.
  std::uint32_t count = 0;
  // Native counter ticks per second.
  std::uint32_t ticks_per_second = 0;
};

// Reads the native system tick and its period. This is an elapsed-time source,
// not a wall clock; readings far enough apart to wrap need another time source.
absl::StatusOr<TickReading> ReadTickCounter();

// Reads the native fast counter and its frequency. Counter availability and
// resolution depend on the actual device or emulator profile.
absl::StatusOr<FastCounterReading> ReadFastCounter();

}  // namespace symbian::api::system

#endif  // SYMBIAN_API_SYSTEM_COUNTERS_H_
