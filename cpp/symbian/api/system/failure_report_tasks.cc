// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "failure_report_tasks.h"

#include <utility>

#include "symbian/concurrency/task_group.h"
#include "thread/boost_primitives.h"

namespace symbian::api::system::internal {

absl::Status WaitForFailureReportTasks(concurrency::Task presentation,
                                       concurrency::Task persistence) {
  concurrency::TaskGroup tasks;
  tasks.Add(std::move(presentation));
  tasks.Add(std::move(persistence));
  const concurrency::Task finished = tasks.Finish();
  thread::Mutex mu;
  thread::CondVar ready;
  bool done = false;
  absl::Status status;
  finished.OnReady([&](const absl::StatusOr<concurrency::Unit>& result) {
    thread::MutexLock lock(&mu);
    status = result.status();
    done = true;
    ready.SignalAll();
  });
  thread::MutexLock lock(&mu);
  while (!done) {
    ready.Wait(&mu);
  }
  return status;
}

}  // namespace symbian::api::system::internal
