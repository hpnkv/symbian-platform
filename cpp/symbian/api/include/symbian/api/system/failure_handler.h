// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_API_SYSTEM_FAILURE_HANDLER_H_
#define SYMBIAN_API_SYSTEM_FAILURE_HANDLER_H_

#include <span>
#include <functional>
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

// Runs a top-level application entry. On a returned error, presents its status
// and latest logs in a scrollable window with Exit and Copy buttons. A process
// panic or failure before this function runs cannot be recovered in-process.
// Returns zero on success and one after the failure view closes or cannot open.
int RunWithFailureHandler(FailureHandledEntry entry,
                          const FailureHandlerOptions& options = {});

// Displays a returned failure after the application's own window has closed.
// The view owns its window and blocks until Exit is chosen.
absl::Status ShowFailureReport(const absl::Status& error,
                               const FailureHandlerOptions& options = {});

}  // namespace symbian::api::system

#endif  // SYMBIAN_API_SYSTEM_FAILURE_HANDLER_H_
