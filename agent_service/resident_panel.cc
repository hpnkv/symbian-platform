// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "resident_panel.h"

#include <absl/base/nullability.h>

#include "native_resident_panel.h"
#include "symbian/native_status.h"

namespace agent_service {

absl::Status RunResidentPanel(const ResidentPanelOptions& options,
                              std::atomic<bool>* absl_nonnull stop_requested) {
  if (options.app_uid == 0 || options.property_category == 0 ||
      options.foreground_key == 0 || options.caption == nullptr ||
      options.heading == nullptr || options.state == nullptr ||
      options.back_label == nullptr || options.stop_label == nullptr) {
    return absl::InvalidArgumentError("Invalid resident panel options");
  }
  const NativeResidentPanelOptions native{
      options.app_uid,    options.property_category, options.foreground_key,
      options.caption,    options.heading,           options.state,
      options.back_label,
      options.stop_label,
      options.heading_provider ? &options.heading_provider : nullptr,
      options.heading_provider
          ? +[](const void* absl_nonnull context) -> const char* absl_nullable {
              return (*static_cast<
                      const std::function<const char* absl_nullable()>*>(
                  context))();
            }
          : nullptr};
  return symbian::StatusFromNativeError(
      SymbianDeviceRunResidentPanel(&native, stop_requested), "Resident panel");
}

absl::Status RequestResidentPanelForeground(std::int32_t category,
                                            std::uint32_t key) {
  return symbian::StatusFromNativeError(
      SymbianDeviceRequestResidentPanelForeground(category, key),
      "Panel foreground signal");
}

}  // namespace agent_service
