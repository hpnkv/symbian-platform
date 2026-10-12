// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_API_SYSTEM_FAILURE_VIEW_THREAD_H_
#define SYMBIAN_API_SYSTEM_FAILURE_VIEW_THREAD_H_

#include <functional>

#include "absl/status/status.h"

namespace symbian::api::system::internal {

// Establishes the native cleanup stack on the dedicated SDK UI worker.
// All window, font and clipboard owners stay on this thread. Scheduling is
// exclusively owned by the SDK's A11 worker, not a native active scheduler.
absl::Status RunFailureView(const std::function<absl::Status()>& present);

}  // namespace symbian::api::system::internal

#endif  // SYMBIAN_API_SYSTEM_FAILURE_VIEW_THREAD_H_
