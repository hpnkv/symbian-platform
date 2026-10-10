// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "symbian/api/display/display.h"

#include "native_display.h"
#include "symbian/native_status.h"

namespace symbian::api::display {

absl::StatusOr<DisplayGeometry> ReadPrimaryDisplayGeometry() {
  NativeDisplayReading native;
  SymbianDeviceReadPrimaryDisplay(&native);
  if (native.width_result != 0) {
    return symbian::StatusFromNativeError(native.width_result,
                                          "HAL display width");
  }
  if (native.height_result != 0) {
    return symbian::StatusFromNativeError(native.height_result,
                                          "HAL display height");
  }
  if (native.width_pixels <= 0 || native.height_pixels <= 0) {
    return absl::FailedPreconditionError("HAL display size is invalid");
  }
  DisplayGeometry geometry{
      .width_pixels = native.width_pixels,
      .height_pixels = native.height_pixels,
  };
  if (native.width_twips_result == 0 && native.width_twips > 0) {
    geometry.width_twips = native.width_twips;
  }
  if (native.height_twips_result == 0 && native.height_twips > 0) {
    geometry.height_twips = native.height_twips;
  }
  return geometry;
}

absl::StatusOr<bool> ReadTouchscreenPresence() {
  int present = 0;
  if (const int result = SymbianDeviceReadTouchscreenPresence(&present);
      result != 0) {
    return symbian::StatusFromNativeError(result, "HAL touchscreen presence");
  }
  return present != 0;
}

}  // namespace symbian::api::display
