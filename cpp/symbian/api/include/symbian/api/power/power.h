// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_API_POWER_POWER_H_
#define SYMBIAN_API_POWER_POWER_H_

#include <optional>

#include "absl/status/statusor.h"

namespace symbian::api::power {

// The platform's qualitative battery state. This is not a charge percentage.
enum class BatteryCondition { kEmpty, kReplace, kLow, kGood };

// A point-in-time collection of independently supported power observations.
struct PowerSnapshot {
  // Whether the platform reports sufficient power to continue operating.
  std::optional<bool> power_good;
  // Whether an external supply is in use; charging is not implied.
  std::optional<bool> external_power;
  // Main battery's qualitative state, if the platform exposes it.
  std::optional<BatteryCondition> battery;
};

// Reads supported native power attributes. Unsupported fields remain empty.
// Returns a status if no attribute can be read. This is a synchronous query;
// schedule it on a worker when the event thread must stay bounded-fast.
absl::StatusOr<PowerSnapshot> ReadPowerSnapshot();

}  // namespace symbian::api::power

#endif  // SYMBIAN_API_POWER_POWER_H_
