// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef AGENT_SERVICE_RESIDENT_PANEL_H_
#define AGENT_SERVICE_RESIDENT_PANEL_H_

#include <atomic>
#include <cstdint>
#include <functional>

#include <absl/base/nullability.h>

#include "absl/status/status.h"

namespace agent_service {

/**
 * @brief Minimal Window Server panel for a manually started resident service.
 *
 * Runs on its own guest thread. BACK lowers the window group without stopping
 * the service; STOP sets stop_requested. A second app launch can call
 * RequestResidentPanelForeground with the same property identity. Labels use
 * the built-in uppercase bitmap alphabet; unsupported glyphs are omitted.
 * All strings must remain valid until RunResidentPanel returns.
 * heading_provider may return a different static label as service state
 * changes; it is called on the panel's Window Server thread. The panel checks
 * it on the existing wake timer and redraws only when the label changes.
 */
struct ResidentPanelOptions {
  std::uint32_t app_uid = 0;
  std::int32_t property_category = 0;
  std::uint32_t foreground_key = 0;
  const char* absl_nonnull caption = "Resident service";
  const char* absl_nonnull heading = "SERVICE";
  const char* absl_nonnull state = "RUNNING";
  const char* absl_nonnull back_label = "BACK";
  const char* absl_nonnull stop_label = "STOP";
  std::function<const char* absl_nullable()> heading_provider;
};

/** @brief Run a resident panel until STOP or an external stop request. */
absl::Status RunResidentPanel(const ResidentPanelOptions& options,
                              std::atomic<bool>* absl_nonnull stop_requested);

/** @brief Raise the window group owned by an existing panel instance. */
absl::Status RequestResidentPanelForeground(std::int32_t category,
                                            std::uint32_t key);

}  // namespace agent_service

#endif  // AGENT_SERVICE_RESIDENT_PANEL_H_
