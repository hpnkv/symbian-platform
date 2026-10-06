// Copyright 2026 The Action Engine Authors and the Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0 (the "License");
// http://www.apache.org/licenses/LICENSE-2.0
//
// No-exceptions host adaptation of A11 cpp/thread/thread/boost_primitives.cc
// at fcccb8cb6e1e67d7ba0822ac14cece9ee4c7091b. A11's forced-unwind
// fiber lifecycle and thread pool are separate gates, not silently copied.

#include "thread/boost_primitives.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>

#include <absl/base/nullability.h>
#include <boost/fiber/condition_variable.hpp>
#include <boost/fiber/mutex.hpp>
#include <boost/fiber/operations.hpp>

#include "absl/time/clock.h"

namespace thread {

struct Mutex::Impl {
  boost::fibers::mutex mutex;
};

Mutex::Mutex() {
  static_assert(sizeof(Impl) <= kImplSize);
  static_assert(alignof(Impl) <= alignof(std::max_align_t));
  std::construct_at(reinterpret_cast<Impl*>(impl_));
}

Mutex::~Mutex() {
  std::destroy_at(GetImpl());
}

Mutex::Impl* absl_nonnull Mutex::GetImpl() {
  return std::launder(reinterpret_cast<Impl*>(impl_));
}

void Mutex::Lock() noexcept {
  GetImpl()->mutex.lock();
}

void Mutex::Unlock() noexcept {
  GetImpl()->mutex.unlock();
}

struct CondVar::Impl {
  std::atomic<std::uint32_t> generation{0};
  boost::fibers::condition_variable_any condition;
};

CondVar::CondVar() {
  static_assert(sizeof(Impl) <= kImplSize);
  static_assert(alignof(Impl) <= alignof(std::max_align_t));
  std::construct_at(reinterpret_cast<Impl*>(impl_));
}

CondVar::~CondVar() {
  std::destroy_at(GetImpl());
}

CondVar::Impl* absl_nonnull CondVar::GetImpl() {
  return std::launder(reinterpret_cast<Impl*>(impl_));
}

void CondVar::Wait(Mutex* absl_nonnull mu) noexcept {
  GetImpl()->condition.wait(mu->GetImpl()->mutex);
}

bool CondVar::WaitWithDeadline(Mutex* absl_nonnull mu,
                               absl::Time deadline) noexcept {
  if (deadline == absl::InfiniteFuture()) {
    Wait(mu);
    return false;
  }
  return WaitWithTimeout(mu, deadline - absl::Now());
}

bool CondVar::WaitWithTimeout(Mutex* absl_nonnull mu,
                              absl::Duration remaining) noexcept {
  if (remaining == absl::InfiniteDuration()) {
    Wait(mu);
    return false;
  }
  if (remaining <= absl::ZeroDuration()) {
    return true;
  }
  Impl* absl_nonnull impl = GetImpl();
  const std::uint32_t observed =
      impl->generation.load(std::memory_order_acquire);
  while (remaining > absl::ZeroDuration()) {
    const absl::Duration slice =
        remaining < absl::Hours(1) ? remaining : absl::Hours(1);
    const auto start = std::chrono::steady_clock::now();
    impl->condition.wait_for(
        mu->GetImpl()->mutex,
        std::chrono::nanoseconds(absl::ToInt64Nanoseconds(slice)));
    if (impl->generation.load(std::memory_order_acquire) != observed) {
      return false;
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - start);
    remaining -= absl::Nanoseconds(elapsed.count());
  }
  return true;
}

void CondVar::Signal() noexcept {
  GetImpl()->generation.fetch_add(1, std::memory_order_release);
  GetImpl()->condition.notify_one();
}

void CondVar::SignalAll() noexcept {
  GetImpl()->generation.fetch_add(1, std::memory_order_release);
  GetImpl()->condition.notify_all();
}

void SleepFor(absl::Duration duration) {
  if (duration <= absl::ZeroDuration()) {
    boost::this_fiber::yield();
    return;
  }
  while (duration > absl::ZeroDuration()) {
    const absl::Duration slice =
        duration < absl::Hours(1) ? duration : absl::Hours(1);
    boost::this_fiber::sleep_for(
        std::chrono::nanoseconds(absl::ToInt64Nanoseconds(slice)));
    duration -= slice;
  }
}

}  // namespace thread
