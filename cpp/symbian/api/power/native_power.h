// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_API_POWER_NATIVE_POWER_H_
#define SYMBIAN_API_POWER_NATIVE_POWER_H_

namespace symbian::api::power {

// Each result is the native error for its paired value; zero means valid.
struct NativePowerReading {
  int power_good_result = -5;
  int power_good = 0;
  int external_power_result = -5;
  int external_power = 0;
  int battery_result = -5;
  int battery = 0;
};

extern "C" void SymbianDeviceReadPower(NativePowerReading* reading);

}  // namespace symbian::api::power

#endif  // SYMBIAN_API_POWER_NATIVE_POWER_H_
