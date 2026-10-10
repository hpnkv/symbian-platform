// Copyright 2026 The Action Engine Authors.
// Licensed under the Apache License, Version 2.0 (the "License");
// http://www.apache.org/licenses/LICENSE-2.0
//
// Guest ARM-fiber adaptation of cpp/thread/thread/boost_primitives.h at
// fcccb8cb6e1e67d7ba0822ac14cece9ee4c7091b. Only the creating event
// thread pumps guest fibers. OS-thread callers retain normal blocking locks.

#ifndef THREAD_BOOST_PRIMITIVES_H_
#define THREAD_BOOST_PRIMITIVES_H_

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>

#include <absl/base/nullability.h>

#include "absl/base/thread_annotations.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "thread/fiber.h"

namespace thread {

inline constexpr bool kCanParkAwait = true;

inline bool CanParkAwait() noexcept {
  return Fiber::Current() != nullptr;
}

class ABSL_LOCKABLE Mutex {
 public:
  Mutex() = default;
  Mutex(const Mutex&) = delete;
  Mutex& operator=(const Mutex&) = delete;

  void Lock() noexcept ABSL_EXCLUSIVE_LOCK_FUNCTION() {
    Fiber* absl_nullable fiber = Fiber::Current();
    if (fiber == nullptr) {
      mu_.lock();
      return;
    }
    while (!mu_.try_lock()) {
      fiber->scheduler_.PreparePark(
          fiber, std::chrono::steady_clock::time_point::max());
      {
        std::lock_guard guard(waiters_mu_);
        waiters_.push_back(fiber);
        // Unlock may have preceded registration. Retry under the waiter
        // guard so the fiber cannot miss the only wakeup.
        if (mu_.try_lock()) {
          waiters_.pop_back();
          fiber->scheduler_.CancelPark(fiber);
          return;
        }
      }
      fiber->scheduler_.Suspend(fiber);
      std::lock_guard guard(waiters_mu_);
      if (auto it = std::find(waiters_.begin(), waiters_.end(), fiber);
          it != waiters_.end()) {
        waiters_.erase(it);
      }
    }
  }

  void Unlock() noexcept ABSL_UNLOCK_FUNCTION() {
    Fiber* absl_nullable fiber = nullptr;
    {
      std::lock_guard guard(waiters_mu_);
      mu_.unlock();
      if (!waiters_.empty()) {
        fiber = waiters_.front();
        waiters_.pop_front();
      }
    }
    if (fiber != nullptr) {
      fiber->scheduler_.Wake(fiber);
    }
  }

  void lock() noexcept ABSL_EXCLUSIVE_LOCK_FUNCTION() { Lock(); }

  void unlock() noexcept ABSL_UNLOCK_FUNCTION() { Unlock(); }

 private:
  friend class CondVar;
  std::mutex mu_;
  std::mutex waiters_mu_;
  std::deque<Fiber* absl_nonnull> waiters_;
};

class ABSL_SCOPED_LOCKABLE MutexLock {
 public:
  explicit MutexLock(Mutex* absl_nonnull mu) ABSL_EXCLUSIVE_LOCK_FUNCTION(mu)
      : mu_(mu) {
    mu_->Lock();
  }

  MutexLock(const MutexLock&) = delete;
  MutexLock& operator=(const MutexLock&) = delete;

  ~MutexLock() ABSL_UNLOCK_FUNCTION() { mu_->Unlock(); }

 private:
  Mutex* absl_nonnull mu_;
};

class CondVar {
 public:
  CondVar() = default;
  CondVar(const CondVar&) = delete;
  CondVar& operator=(const CondVar&) = delete;

  void Wait(Mutex* absl_nonnull mu) noexcept {
    if (Fiber::Current() != nullptr) {
      WaitWithTimeout(mu, absl::InfiniteDuration());
      return;
    }
    std::unique_lock<std::mutex> lock(mu->mu_, std::adopt_lock);
    cv_.wait(lock);
    lock.release();
  }

  bool WaitWithDeadline(Mutex* absl_nonnull mu, absl::Time deadline) noexcept {
    if (deadline == absl::InfiniteFuture()) {
      Wait(mu);
      return false;
    }
    return WaitWithTimeout(mu, deadline - absl::Now());
  }

  bool WaitWithTimeout(Mutex* absl_nonnull mu,
                       absl::Duration remaining) noexcept {
    if (Fiber* absl_nullable fiber = Fiber::Current()) {
      if (remaining <= absl::ZeroDuration()) {
        return true;
      }
      const bool infinite = remaining == absl::InfiniteDuration();
      while (infinite || remaining > absl::ZeroDuration()) {
        const auto start = std::chrono::steady_clock::now();
        auto deadline = std::chrono::steady_clock::time_point::max();
        if (!infinite) {
          const absl::Duration safe = remaining < absl::Hours(24 * 365)
                                          ? remaining
                                          : absl::Hours(24 * 365);
          deadline =
              start + std::chrono::nanoseconds(absl::ToInt64Nanoseconds(safe));
        }
        Waiter waiter{.fiber = fiber, .signalled = false};
        {
          std::lock_guard guard(waiters_mu_);
          fiber->scheduler_.PreparePark(fiber, deadline);
          waiters_.push_back(&waiter);
        }
        mu->Unlock();
        fiber->scheduler_.Suspend(fiber);
        bool signalled = false;
        {
          std::lock_guard guard(waiters_mu_);
          if (auto it = std::find(waiters_.begin(), waiters_.end(), &waiter);
              it != waiters_.end()) {
            waiters_.erase(it);
          }
          signalled = waiter.signalled;
        }
        mu->Lock();
        if (signalled) {
          return false;
        }
        if (!infinite) {
          const auto elapsed =
              std::chrono::duration_cast<std::chrono::nanoseconds>(
                  std::chrono::steady_clock::now() - start);
          remaining -= absl::Nanoseconds(elapsed.count());
        }
      }
      return true;
    }
    if (remaining == absl::InfiniteDuration()) {
      Wait(mu);
      return false;
    }
    if (remaining <= absl::ZeroDuration()) {
      return true;
    }
    const std::uint32_t observed = generation_.load(std::memory_order_acquire);
    std::unique_lock<std::mutex> lock(mu->mu_, std::adopt_lock);
    bool signalled = false;
    while (remaining > absl::ZeroDuration()) {
      // Convert the accepted wall deadline once. Short steady-clock slices
      // avoid overflowing libc++/pthread timer ranges on distant deadlines.
      const absl::Duration slice =
          remaining < absl::Hours(1) ? remaining : absl::Hours(1);
      const auto start = std::chrono::steady_clock::now();
      cv_.wait_for(lock,
                   std::chrono::nanoseconds(absl::ToInt64Nanoseconds(slice)));
      if (generation_.load(std::memory_order_acquire) != observed) {
        signalled = true;
        break;
      }
      const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::steady_clock::now() - start);
      remaining -= absl::Nanoseconds(elapsed.count());
    }
    lock.release();
    return !signalled;
  }

  void Signal() noexcept {
    generation_.fetch_add(1, std::memory_order_release);
    Scheduler* absl_nullable scheduler = nullptr;
    bool notify = false;
    {
      std::lock_guard guard(waiters_mu_);
      if (!waiters_.empty()) {
        Waiter* absl_nonnull waiter = waiters_.front();
        waiters_.pop_front();
        waiter->signalled = true;
        scheduler = &waiter->fiber->scheduler_;
        notify = scheduler->WakeWithoutNotify(waiter->fiber);
      }
    }
    if (notify) {
      scheduler->NotifyReady();
    }
    cv_.notify_one();
  }

  void SignalAll() noexcept {
    generation_.fetch_add(1, std::memory_order_release);
    std::deque<Scheduler* absl_nonnull> schedulers;
    {
      std::lock_guard guard(waiters_mu_);
      while (!waiters_.empty()) {
        Waiter* absl_nonnull waiter = waiters_.front();
        waiters_.pop_front();
        waiter->signalled = true;
        Scheduler* absl_nonnull scheduler = &waiter->fiber->scheduler_;
        if (scheduler->WakeWithoutNotify(waiter->fiber)) {
          schedulers.push_back(scheduler);
        }
      }
    }
    for (Scheduler* absl_nonnull scheduler : schedulers) {
      scheduler->NotifyReady();
    }
    cv_.notify_all();
  }

 private:
  struct Waiter {
    Fiber* absl_nonnull fiber;
    bool signalled;
  };

  std::mutex waiters_mu_;
  std::deque<Waiter* absl_nonnull> waiters_;
  std::atomic<std::uint32_t> generation_{0};
  std::condition_variable cv_;
};

inline void SleepFor(absl::Duration duration) {
  if (Fiber::Current() != nullptr) {
    Fiber::SleepFor(duration);
    return;
  }
  if (duration <= absl::ZeroDuration()) {
    std::this_thread::yield();
    return;
  }
  while (duration > absl::ZeroDuration()) {
    const absl::Duration slice =
        duration < absl::Hours(1) ? duration : absl::Hours(1);
    std::this_thread::sleep_for(
        std::chrono::nanoseconds(absl::ToInt64Nanoseconds(slice)));
    duration -= slice;
  }
}

}  // namespace thread

#endif  // THREAD_BOOST_PRIMITIVES_H_
