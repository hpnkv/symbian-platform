// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_API_DISPLAY_NATIVE_RESIDENT_PANEL_H_
#define SYMBIAN_API_DISPLAY_NATIVE_RESIDENT_PANEL_H_

#include <stdint.h>

struct NativeResidentPanelOptions {
  uint32_t app_uid;
  int32_t property_category;
  uint32_t foreground_key;
  const char* caption;
  const char* heading;
  const char* state;
  const char* back_label;
  const char* stop_label;
};

extern "C" int SymbianDeviceRunResidentPanel(
    const NativeResidentPanelOptions* options, void* stop_requested);
extern "C" int SymbianDeviceRequestResidentPanelForeground(int category,
                                                              unsigned key);

#endif  // SYMBIAN_API_DISPLAY_NATIVE_RESIDENT_PANEL_H_
