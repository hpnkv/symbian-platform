// Copyright 2026 The A11 Authors and the Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.
// Adapted from A11 cpp/thread/thread/thread_pool.cc, whose scheduler caps an
// idle park and drops a registered host lock for that park. Boost stays private.

#include "thread/executor.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <mutex>
#include <utility>

#include <absl/base/nullability.h>
#include <boost/fiber/algo/algorithm.hpp>
#include <boost/fiber/context.hpp>
#include <boost/fiber/operations.hpp>
#include <boost/fiber/scheduler.hpp>

namespace thread {
namespace {

std::atomic<const SchedulerParkGuard* absl_nullable>& ParkGuard() {
  static std::atomic<const SchedulerParkGuard* absl_nullable> guard{nullptr};
  return guard;
}

class ParkWithoutHostLock {
 public:
  ParkWithoutHostLock() : guard_(ParkGuard().load(std::memory_order_acquire)) {
    if (guard_ != nullptr) {
      held_ = guard_->release();
    }
  }

  ~ParkWithoutHostLock() {
    if (guard_ != nullptr) {
      guard_->acquire(held_);
    }
  }

  ParkWithoutHostLock(const ParkWithoutHostLock&) = delete;
  ParkWithoutHostLock& operator=(const ParkWithoutHostLock&) = delete;

 private:
  const SchedulerParkGuard* absl_nullable guard_;
  void* absl_nullable held_ = nullptr;
};

class RoundRobinPolicy final : public SchedulerPolicy {
 public:
  size_t PickNext(size_t) noexcept override { return 0; }
};

thread_local bool scheduler_installed = false;
thread_local std::shared_ptr<SchedulerPolicy> scheduler_policy;

class SdkAlgorithm final : public boost::fibers::algo::algorithm {
 public:
  explicit SdkAlgorithm(std::shared_ptr<SchedulerPolicy> policy)
      : policy_(std::move(policy)) {}

  void awakened(
      boost::fibers::context* absl_nonnull context) noexcept override {
    context->ready_link(ready_queue_);
  }

  boost::fibers::context* absl_nullable pick_next() noexcept override {
    if (ready_queue_.empty()) {
      return nullptr;
    }
    const size_t choice = policy_->PickNext(ready_queue_.size());
    if (choice >= ready_queue_.size()) {
      std::abort();
    }
    auto it = ready_queue_.begin();
    std::advance(it, static_cast<std::ptrdiff_t>(choice));
    boost::fibers::context* absl_nonnull selected = &*it;
    ready_queue_.erase(it);
    return selected;
  }

  bool has_ready_fibers() const noexcept override {
    return !ready_queue_.empty();
  }

  void suspend_until(
      const std::chrono::steady_clock::time_point& deadline) noexcept override {
    // A11 caps idle parks to recheck work. The guard drops CPython's GIL only
    // if this thread actually owns it, then restores that exact thread state.
    const auto cap =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(50);
    const ParkWithoutHostLock without_host_lock;
    std::unique_lock lock(mu_);
    cv_.wait_until(lock, std::min(deadline, cap),
                   [this] { return wake_sequence_ != consumed_sequence_; });
    consumed_sequence_ = wake_sequence_;
  }

  void notify() noexcept override {
    std::unique_lock lock(mu_);
    ++wake_sequence_;
    lock.unlock();
    cv_.notify_all();
  }

 private:
  std::shared_ptr<SchedulerPolicy> policy_;
  boost::fibers::scheduler::ready_queue_type ready_queue_;
  std::mutex mu_;
  std::condition_variable cv_;
  uint64_t wake_sequence_ = 0;
  uint64_t consumed_sequence_ = 0;
};

}  // namespace

void SetSchedulerParkGuard(SchedulerParkGuard guard) {
  const SchedulerParkGuard* absl_nullable installed = nullptr;
  if (guard.release != nullptr && guard.acquire != nullptr) {
    // A concurrently active park may still hold the old pair. Match A11's
    // process-lifetime callbacks rather than racing deallocation.
    installed = new SchedulerParkGuard(std::move(guard));
  }
  ParkGuard().store(installed, std::memory_order_release);
}

absl::Status SetCurrentSchedulerPolicy(
    std::shared_ptr<SchedulerPolicy> policy) {
  if (scheduler_installed) {
    return absl::FailedPreconditionError(
        "The calling OS thread already owns a fiber scheduler");
  }
  if (policy == nullptr) {
    return absl::InvalidArgumentError("Scheduler policy is null");
  }
  scheduler_policy = std::move(policy);
  return absl::OkStatus();
}

namespace internal {
void EnsureCurrentScheduler() {
  if (scheduler_installed) {
    return;
  }
  if (scheduler_policy == nullptr) {
    scheduler_policy = std::make_shared<RoundRobinPolicy>();
  }
  boost::fibers::use_scheduling_algorithm<SdkAlgorithm>(scheduler_policy);
  scheduler_installed = true;
}
}  // namespace internal

}  // namespace thread
