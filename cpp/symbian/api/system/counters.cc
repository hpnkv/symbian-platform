// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "symbian/api/system/counters.h"

#include "symbian/native_status.h"
#include "symbian/runtime.h"

namespace symbian::api::system {

absl::StatusOr<TickReading> ReadTickCounter() {
  const int period = SymbianRuntimeTickPeriodMicros();
  if (period < 0) {
    return symbian::StatusFromNativeError(period, "System tick period");
  }
  if (period == 0) {
    return absl::FailedPreconditionError("System tick period is zero");
  }
  return TickReading{
      .count = SymbianRuntimeTickCount(),
      .period = absl::Microseconds(period),
  };
}

absl::StatusOr<FastCounterReading> ReadFastCounter() {
  const int frequency = SymbianRuntimeFastCounterFrequency();
  if (frequency < 0) {
    return symbian::StatusFromNativeError(frequency, "Fast counter frequency");
  }
  if (frequency == 0) {
    return absl::FailedPreconditionError("Fast counter frequency is zero");
  }
  return FastCounterReading{
      .count = SymbianRuntimeFastCounter(),
      .ticks_per_second = static_cast<std::uint32_t>(frequency),
  };
}

}  // namespace symbian::api::system
