// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include <memory>
#include <utility>

#include <absl/base/nullability.h>

#include "failure_report_tasks.h"
#include "symbian/concurrency/worker_executor.h"

namespace symbian::api::system::internal {
namespace {

concurrency::Task StartFailureTask(
    concurrency::WorkerExecutor* absl_nonnull worker,
    std::function<absl::Status()> work) {
  auto completion = std::make_shared<concurrency::Promise<concurrency::Unit>>();
  const concurrency::Task task = completion->future();
  // Use separate OS workers: synchronous native file and Window Server waits
  // must never block the other task's scheduler. Native text/clipboard calls
  // need more stack than the default small compute fiber.
  const concurrency::Task dispatched = worker->PostFiber(
      [completion, work = std::move(work)] {
        const absl::Status status = work();
        if (status.ok()) {
          completion->SetValue(concurrency::Unit{});
        } else {
          completion->SetError(status);
        }
      },
      128 * 1024);
  dispatched.OnReady(
      [completion](const absl::StatusOr<concurrency::Unit>& result) {
        if (!result.ok()) {
          completion->SetError(result.status());
        }
      });
  return task;
}

}  // namespace

absl::Status RunFailureReportTasks(std::function<absl::Status()> present,
                                   std::function<absl::Status()> persist) {
  concurrency::WorkerExecutor writer(1);
  const concurrency::Task persistence =
      StartFailureTask(&writer, std::move(persist));
  if (!present) {
    return WaitForFailureReportTasks(concurrency::ReadyTask(), persistence);
  }
  concurrency::WorkerExecutor viewer(1);
  const concurrency::Task presentation =
      StartFailureTask(&viewer, std::move(present));
  // The UI task destroys its window before completing. A slow write then
  // drains without retaining the window or blocking interaction with the OS.
  // A fatal CHECK must not kill the process until persistence has finished.
  return WaitForFailureReportTasks(presentation, persistence);
}

}  // namespace symbian::api::system::internal
