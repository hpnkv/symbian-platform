// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_API_SYSTEM_FAILURE_REPORT_TASKS_H_
#define SYMBIAN_API_SYSTEM_FAILURE_REPORT_TASKS_H_

#include <functional>

#include "absl/status/status.h"
#include "symbian/concurrency/future.h"

namespace symbian::api::system::internal {

// Runs independent tasks on separate SDK workers, then joins both. An empty
// presentation callback selects persistence only for a background failure.
// The presentation callback must destroy its window before returning.
absl::Status RunFailureReportTasks(std::function<absl::Status()> present,
                                   std::function<absl::Status()> persist);

// Joins both tasks without cancelling either on error. Presentation owns and
// destroys its window before completing. A caller outside a fiber sleeps on
// an OS condition variable; a fiber caller parks on the A11 scheduler.
// Presentation errors take precedence over persistence errors.
absl::Status WaitForFailureReportTasks(concurrency::Task presentation,
                                       concurrency::Task persistence);

}  // namespace symbian::api::system::internal

#endif  // SYMBIAN_API_SYSTEM_FAILURE_REPORT_TASKS_H_
