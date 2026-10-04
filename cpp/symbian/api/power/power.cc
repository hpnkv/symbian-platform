// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "symbian/api/power/power.h"

#include "native_power.h"
#include "symbian/native_status.h"

namespace symbian::api::power {

absl::StatusOr<PowerSnapshot> ReadPowerSnapshot() {
  NativePowerReading native;
  SymbianDeviceReadPower(&native);
  PowerSnapshot snapshot;
  if (native.power_good_result == 0 &&
      (native.power_good == 0 || native.power_good == 1)) {
    snapshot.power_good = native.power_good == 1;
  }
  if (native.external_power_result == 0 &&
      (native.external_power == 0 || native.external_power == 1)) {
    snapshot.external_power = native.external_power == 1;
  }
  if (native.battery_result == 0 && native.battery >= 0 &&
      native.battery <= 3) {
    snapshot.battery = static_cast<BatteryCondition>(native.battery);
  }
  if (snapshot.power_good || snapshot.external_power || snapshot.battery) {
    return snapshot;
  }
  if ((native.power_good_result == 0 && native.power_good != 0 &&
       native.power_good != 1) ||
      (native.external_power_result == 0 && native.external_power != 0 &&
       native.external_power != 1) ||
      (native.battery_result == 0 &&
       (native.battery < 0 || native.battery > 3))) {
    return absl::FailedPreconditionError("HAL returned invalid power values");
  }
  if (native.power_good_result != 0) {
    return symbian::StatusFromNativeError(native.power_good_result,
                                          "HAL power state");
  }
  if (native.external_power_result != 0) {
    return symbian::StatusFromNativeError(native.external_power_result,
                                          "HAL external power");
  }
  if (native.battery_result != 0) {
    return symbian::StatusFromNativeError(native.battery_result,
                                          "HAL battery state");
  }
  return absl::FailedPreconditionError("HAL returned no valid power values");
}

}  // namespace symbian::api::power
