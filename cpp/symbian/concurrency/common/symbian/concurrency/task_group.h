// Copyright 2026 Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0 (the "License");
// http://www.apache.org/licenses/LICENSE-2.0
//
// Guest stackless task group built on the pinned A11 Promise/Future and
// all-result fan-in adaptation. It owns no scheduler or native request queue.

#ifndef SYMBIAN_CONCURRENCY_TASK_GROUP_H_
#define SYMBIAN_CONCURRENCY_TASK_GROUP_H_

#include <utility>
#include <vector>

#include <absl/status/status_macros.h>

#include "symbian/concurrency/parallel.h"

namespace symbian::concurrency {

class TaskGroup {
 public:
  TaskGroup() = default;
  TaskGroup(const TaskGroup&) = delete;
  TaskGroup& operator=(const TaskGroup&) = delete;

  ~TaskGroup() {
    if (!closed_) {
      Cancel();
    }
  }

  // A child is already running or registered with its OS completion owner.
  // Add does not create a worker or transfer ownership of native buffers.
  bool Add(Task child) {
    if (closed_ || !child.valid()) {
      return false;
    }
    children_.push_back(std::move(child));
    return true;
  }

  void Cancel() const {
    if (closed_) {
      joined_.Cancel();
      return;
    }
    for (const auto& child : children_) {
      child.Cancel();
    }
  }

  // Finish is the asynchronous join point. Every child settles before the
  // returned Task publishes the first error or Unit success. Cancel on that
  // Task forwards to all children, but completion remains producer-owned.
  Task Finish() {
    if (closed_) {
      return joined_;
    }
    closed_ = true;
    auto joined = JoinAll(std::move(children_));
    joined_ =
        Then(joined,
             [](const absl::StatusOr<std::vector<absl::StatusOr<Unit>>>& all)
                 -> absl::StatusOr<Unit> {
               ABSL_ASSIGN_OR_RETURN(const auto& results, all);
               for (const auto& child : results) {
                 ABSL_RETURN_IF_ERROR(child.status());
               }
               return Unit{};
             });
    return joined_;
  }

 private:
  std::vector<Task> children_;
  Task joined_;
  bool closed_ = false;
};

}  // namespace symbian::concurrency

#endif  // SYMBIAN_CONCURRENCY_TASK_GROUP_H_
