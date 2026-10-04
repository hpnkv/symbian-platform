// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "native_power.h"

#include <hal.h>

namespace symbian::api::power {

extern "C" void SymbianDeviceReadPower(NativePowerReading* reading) {
  if (reading == nullptr) {
    return;
  }
  reading->power_good_result =
      HAL::Get(HALData::EPowerGood, reading->power_good);
  reading->external_power_result =
      HAL::Get(HALData::EPowerExternal, reading->external_power);
  reading->battery_result =
      HAL::Get(HALData::EPowerBatteryStatus, reading->battery);
}

}  // namespace symbian::api::power
