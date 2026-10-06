// Copyright 2026 The Action Engine Authors and the Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0 (the "License");
// http://www.apache.org/licenses/LICENSE-2.0
//
// Boost-free public ABI adapted from A11 cpp/thread/thread/boost_primitives.h
// at fcccb8cb6e1e67d7ba0822ac14cece9ee4c7091b. Boost is private to the
// host implementation; guest builds select a different primitive header.

#ifndef THREAD_BOOST_PRIMITIVES_H_
#define THREAD_BOOST_PRIMITIVES_H_

#include <cstddef>

#include <absl/base/nullability.h>

#include "absl/base/thread_annotations.h"
#include "absl/time/time.h"

namespace thread {

inline constexpr bool kCanParkAwait = true;

inline bool CanParkAwait() noexcept {
  return true;
}

class ABSL_LOCKABLE Mutex {
 public:
  Mutex();
  ~Mutex();
  Mutex(const Mutex&) = delete;
  Mutex& operator=(const Mutex&) = delete;

  void Lock() noexcept ABSL_EXCLUSIVE_LOCK_FUNCTION();
  void Unlock() noexcept ABSL_UNLOCK_FUNCTION();

  void lock() noexcept ABSL_EXCLUSIVE_LOCK_FUNCTION() { Lock(); }

  void unlock() noexcept ABSL_UNLOCK_FUNCTION() { Unlock(); }

 private:
  friend class CondVar;
  struct Impl;
  static constexpr std::size_t kImplSize = 64;
  alignas(std::max_align_t) std::byte impl_[kImplSize];
  Impl* absl_nonnull GetImpl();
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
  CondVar();
  ~CondVar();
  CondVar(const CondVar&) = delete;
  CondVar& operator=(const CondVar&) = delete;

  void Wait(Mutex* absl_nonnull mu) noexcept;
  // Matches A11: true on timeout, false on a signal. Recheck predicates
  // after any wakeup. An accepted absolute deadline becomes steady elapsed
  // time, so later wall-clock adjustments do not change that wait.
  bool WaitWithDeadline(Mutex* absl_nonnull mu, absl::Time deadline) noexcept;
  bool WaitWithTimeout(Mutex* absl_nonnull mu, absl::Duration timeout) noexcept;
  void Signal() noexcept;
  void SignalAll() noexcept;

 private:
  struct Impl;
  static constexpr std::size_t kImplSize = 64;
  alignas(std::max_align_t) std::byte impl_[kImplSize];
  Impl* absl_nonnull GetImpl();
};

void SleepFor(absl::Duration duration);

}  // namespace thread

#endif  // THREAD_BOOST_PRIMITIVES_H_
