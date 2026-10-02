// Copyright 2026 The A11 Authors and the Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.
// Adapted from A11 cpp/thread/thread/executor.h at
// fcccb8cb6e1e67d7ba0822ac14cece9ee4c7091b.

#ifndef THREAD_EXECUTOR_H_
#define THREAD_EXECUTOR_H_

#include <cstddef>
#include <memory>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"

namespace thread {

// Short nonblocking callbacks run on one shared host worker pool without
// committing a fiber stack. A callback must not synchronously wait on work
// that needs the same pool to make progress.
void Post(absl::AnyInvocable<void() &&> work);
void PostAt(absl::Time deadline, absl::AnyInvocable<void() &&> work);

inline void PostAfter(absl::Duration delay,
                      absl::AnyInvocable<void() &&> work) {
  PostAt(absl::Now() + delay, std::move(work));
}

struct SchedulerParkGuard {
  absl::AnyInvocable<void*() const> release;
  absl::AnyInvocable<void(void*) const> acquire;
};

// Installs the host-lock release/reacquire pair used around an idle scheduler
// park. Install before running fibers. Clearing the pair affects future parks;
// an active park retains the old callbacks until it returns.
void SetSchedulerParkGuard(SchedulerParkGuard guard);

// A policy controls ready-fiber order on one OS thread. PickNext must return
// an index less than ready_count and must not block or enter Python.
class SchedulerPolicy {
 public:
  virtual ~SchedulerPolicy() = default;
  virtual size_t PickNext(size_t ready_count) noexcept = 0;
};

// Installs a policy for the calling OS thread before its first SDK fiber.
// Once Boost owns that thread's scheduler, replacing it is not supported.
absl::Status SetCurrentSchedulerPolicy(std::shared_ptr<SchedulerPolicy> policy);

namespace internal {
void EnsureCurrentScheduler();
}

}  // namespace thread

#endif  // THREAD_EXECUTOR_H_
