// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "native_display.h"

#include <absl/base/nullability.h>
#include <hal.h>

namespace symbian::api::display {

extern "C" void SymbianDeviceReadPrimaryDisplay(
    NativeDisplayReading* absl_nullable reading) {
  if (reading == nullptr) {
    return;
  }
  reading->width_result =
      HAL::Get(HALData::EDisplayXPixels, reading->width_pixels);
  reading->height_result =
      HAL::Get(HALData::EDisplayYPixels, reading->height_pixels);
  reading->width_twips_result =
      HAL::Get(HALData::EDisplayXTwips, reading->width_twips);
  reading->height_twips_result =
      HAL::Get(HALData::EDisplayYTwips, reading->height_twips);
}

}  // namespace symbian::api::display
