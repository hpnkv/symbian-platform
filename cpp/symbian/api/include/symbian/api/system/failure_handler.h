// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_API_SYSTEM_FAILURE_HANDLER_H_
#define SYMBIAN_API_SYSTEM_FAILURE_HANDLER_H_

#include <functional>
#include <span>
#include <string_view>

#include <absl/base/nullability.h>

#include "absl/status/status.h"

namespace symbian::api::system {

struct FailureHandlerOptions {
  std::string_view caption = "Application failure";
  // Optional absolute device paths. The newest 16 KiB of each file is shown.
  // Recent DebugLog messages are included automatically.
  std::span<const std::u16string_view> log_paths;
};

using FailureHandledEntry = std::function<absl::Status()>;

// Runs a top-level application entry. On a returned error, saves a report to
// C:\private\<app UID>\failure.txt and presents its status and latest logs in a
// scrollable window with Exit and Copy buttons.
// Returns zero on success and one after the failure view closes or cannot open.
int RunWithFailureHandler(FailureHandledEntry entry,
                          const FailureHandlerOptions& options = {});

// Saves the failure report and displays it after the application's own window
// has closed. The view owns its window and blocks until Exit is chosen.
// SDK executables also install this behavior for Abseil CHECK failures:
// CHECK_OK(Main()) can be used at the integer entry boundary. A fatal CHECK
// in a background app writes the report without opening a window.
absl::Status ShowFailureReport(const absl::Status& error,
                               const FailureHandlerOptions& options = {});

}  // namespace symbian::api::system

#endif  // SYMBIAN_API_SYSTEM_FAILURE_HANDLER_H_
