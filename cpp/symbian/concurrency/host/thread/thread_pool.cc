// Copyright 2026 The A11 Authors and the Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.
// Bounded no-exceptions host adaptation of A11 cpp/thread/thread_pool.cc at
// fcccb8cb6e1e67d7ba0822ac14cece9ee4c7091b. Uses its original MPMC
// WorkQueue; fiber context work stealing and A11 pool tuning remain open.

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <map>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#include "thread/executor.h"
#include "thread/internal/work_queue.h"

namespace thread {
namespace {

using Work = absl::AnyInvocable<void() &&>;
using Clock = std::chrono::steady_clock;

class WorkerPool {
 public:
  WorkerPool() {
    const unsigned int available = std::thread::hardware_concurrency();
    const unsigned int workers = std::clamp(available, 2u, 64u);
    threads_.reserve(workers);
    for (unsigned int index = 0; index < workers; ++index) {
      threads_.emplace_back([this] { Run(); });
    }
  }

  ~WorkerPool() {
    {
      std::lock_guard lock(mu_);
      stopping_ = true;
      wake_sequence_.fetch_add(1, std::memory_order_release);
    }
    cv_.notify_all();
    for (std::thread& worker : threads_) {
      worker.join();
    }
  }

  void Post(Work work) {
    callbacks_.Push(std::move(work));
    WakeOne();
  }

  void PostAt(absl::Time deadline, Work work) {
    if (deadline <= absl::Now()) {
      Post(std::move(work));
      return;
    }
    Clock::time_point monotonic_deadline = Clock::time_point::max();
    if (deadline != absl::InfiniteFuture()) {
      const absl::Duration remaining = deadline - absl::Now();
      const auto limit = Clock::time_point::max() - Clock::now();
      const auto duration =
          std::chrono::nanoseconds(absl::ToInt64Nanoseconds(remaining));
      if (duration < limit) {
        monotonic_deadline = Clock::now() + duration;
      }
    }
    {
      std::lock_guard lock(mu_);
      timers_.emplace(monotonic_deadline, std::move(work));
      wake_sequence_.fetch_add(1, std::memory_order_release);
    }
    if (parked_.load(std::memory_order_acquire) != 0) {
      cv_.notify_one();
    }
  }

 private:
  void WakeOne() {
    {
      std::lock_guard lock(mu_);
      wake_sequence_.fetch_add(1, std::memory_order_release);
    }
    if (parked_.load(std::memory_order_acquire) != 0) {
      cv_.notify_one();
    }
  }

  void Run() {
    while (true) {
      const uint64_t seen = wake_sequence_.load(std::memory_order_acquire);
      Work callback;
      if (callbacks_.Pop(callback)) {
        std::move(callback)();
        continue;
      }
      std::unique_lock lock(mu_);
      if (stopping_) {
        return;
      }
      if (!timers_.empty() && timers_.begin()->first <= Clock::now()) {
        callback = std::move(timers_.begin()->second);
        timers_.erase(timers_.begin());
        lock.unlock();
        std::move(callback)();
        continue;
      }
      const auto next =
          timers_.empty() ? Clock::time_point::max() : timers_.begin()->first;
      parked_.fetch_add(1, std::memory_order_release);
      cv_.wait_until(lock, next, [this, seen] {
        return stopping_ ||
               wake_sequence_.load(std::memory_order_acquire) != seen;
      });
      parked_.fetch_sub(1, std::memory_order_release);
    }
  }

  internal::WorkQueue<Work, 256> callbacks_;
  std::mutex mu_;
  std::condition_variable cv_;
  std::multimap<Clock::time_point, Work> timers_;
  std::atomic<uint64_t> wake_sequence_{0};
  std::atomic<unsigned int> parked_{0};
  bool stopping_ = false;
  std::vector<std::thread> threads_;
};

WorkerPool& Pool() {
  static WorkerPool pool;
  return pool;
}

}  // namespace

void Post(absl::AnyInvocable<void() &&> work) {
  Pool().Post(std::move(work));
}

void PostAt(absl::Time deadline, absl::AnyInvocable<void() &&> work) {
  Pool().PostAt(deadline, std::move(work));
}

}  // namespace thread
