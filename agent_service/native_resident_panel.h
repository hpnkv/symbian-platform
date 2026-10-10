// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef AGENT_SERVICE_NATIVE_RESIDENT_PANEL_H_
#define AGENT_SERVICE_NATIVE_RESIDENT_PANEL_H_

#include <absl/base/nullability.h>
#include <stdint.h>

struct NativeResidentPanelOptions {
  uint32_t app_uid;
  int32_t property_category;
  uint32_t foreground_key;
  const char* absl_nonnull caption;
  const char* absl_nonnull heading;
  const char* absl_nonnull state;
  const char* absl_nonnull back_label;
  const char* absl_nonnull stop_label;
  const void* absl_nullable heading_context;
  const char* absl_nullable (*absl_nullable heading_provider)(
      const void* absl_nonnull context);
};

extern "C" int SymbianDeviceRunResidentPanel(
    const NativeResidentPanelOptions* absl_nonnull options,
    void* absl_nonnull stop_requested);
extern "C" int SymbianDeviceRequestResidentPanelForeground(int category,
                                                           unsigned key);

#endif  // AGENT_SERVICE_NATIVE_RESIDENT_PANEL_H_
