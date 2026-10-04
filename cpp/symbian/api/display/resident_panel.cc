// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "symbian/api/display/resident_panel.h"

#include "native_resident_panel.h"
#include "symbian/native_status.h"

namespace symbian::api::display {

absl::Status RunResidentPanel(const ResidentPanelOptions& options,
                              std::atomic<bool>& stop_requested) {
  if (options.app_uid == 0 || options.property_category == 0 ||
      options.foreground_key == 0 || options.caption == nullptr ||
      options.heading == nullptr || options.state == nullptr ||
      options.back_label == nullptr || options.stop_label == nullptr) {
    return absl::InvalidArgumentError("Invalid resident panel options");
  }
  const NativeResidentPanelOptions native{
      options.app_uid,    options.property_category, options.foreground_key,
      options.caption,    options.heading,           options.state,
      options.back_label, options.stop_label,        options.heading_provider};
  return symbian::StatusFromNativeError(
      SymbianDeviceRunResidentPanel(&native, &stop_requested),
      "Resident panel");
}

absl::Status RequestResidentPanelForeground(std::int32_t category,
                                            std::uint32_t key) {
  return symbian::StatusFromNativeError(
      SymbianDeviceRequestResidentPanelForeground(category, key),
      "Panel foreground signal");
}

}  // namespace symbian::api::display
