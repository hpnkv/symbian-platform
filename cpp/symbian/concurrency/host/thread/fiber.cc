// Copyright 2026 The Action Engine Authors and the Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0 (the "License");
// http://www.apache.org/licenses/LICENSE-2.0
//
// Private Boost backend for the bounded thread::Fiber host API. Boost headers
// and link targets do not appear in the common/public consumer interface.

#include "thread/fiber.h"

#include <atomic>
#include <exception>
#include <thread>

#include <boost/fiber/fiber.hpp>

#include "thread/executor.h"

namespace thread {
namespace {
thread_local Fiber* current_fiber = nullptr;
}  // namespace

struct Fiber::Impl {
  explicit Impl(Fiber* owner, Work work)
      : thread_id(std::this_thread::get_id()),
        fiber([owner, work = std::move(work), this]() mutable {
          Fiber* previous = current_fiber;
          current_fiber = owner;
          std::move(work)();
          current_fiber = previous;
          finished.store(true, std::memory_order_release);
        }) {}

  std::thread::id thread_id;
  std::atomic<bool> cancelled{false};
  std::atomic<bool> finished{false};
  boost::fibers::fiber fiber;
};

Fiber::Fiber(Work work) {
  internal::EnsureCurrentScheduler();
  impl_ = std::make_unique<Impl>(this, std::move(work));
}

Fiber::~Fiber() {
  // A11 requires explicit joining. Destroying an unjoined Boost fiber would
  // terminate or trigger an unverified forced unwind in this no-exceptions SDK.
  if (impl_->fiber.joinable()) {
    std::terminate();
  }
}

void Fiber::Cancel() noexcept {
  impl_->cancelled.store(true, std::memory_order_release);
}

bool Fiber::Cancelled() const noexcept {
  return impl_->cancelled.load(std::memory_order_acquire);
}

bool Fiber::Finished() const noexcept {
  return impl_->finished.load(std::memory_order_acquire);
}

absl::Status Fiber::Join() {
  if (std::this_thread::get_id() != impl_->thread_id) {
    return absl::FailedPreconditionError(
        "Fiber must be joined on its creating OS thread");
  }
  if (!impl_->fiber.joinable()) {
    return absl::FailedPreconditionError("Fiber was already joined");
  }
  impl_->fiber.join();
  return absl::OkStatus();
}

Fiber* Fiber::Current() noexcept {
  return current_fiber;
}

}  // namespace thread
