// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_API_DISPLAY_NATIVE_DISPLAY_H_
#define SYMBIAN_API_DISPLAY_NATIVE_DISPLAY_H_

#include <absl/base/nullability.h>

namespace symbian::api::display {

// Each result is the native error for its paired value; zero means valid.
struct NativeDisplayReading {
  int width_result = -5;
  int width_pixels = 0;
  int height_result = -5;
  int height_pixels = 0;
  int width_twips_result = -5;
  int width_twips = 0;
  int height_twips_result = -5;
  int height_twips = 0;
};

extern "C" void SymbianDeviceReadPrimaryDisplay(
    NativeDisplayReading* absl_nullable reading);

}  // namespace symbian::api::display

#endif  // SYMBIAN_API_DISPLAY_NATIVE_DISPLAY_H_
