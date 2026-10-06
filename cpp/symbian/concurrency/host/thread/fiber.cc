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

#include <absl/base/nullability.h>
#include <boost/fiber/fiber.hpp>

#include "thread/executor.h"
#include "thread/selectables.h"

namespace thread {
namespace {
thread_local Fiber* absl_nullable current_fiber = nullptr;
}  // namespace

struct Fiber::Impl {
  explicit Impl(Fiber* absl_nonnull owner, Work work)
      : thread_id(std::this_thread::get_id()),
        fiber([owner, work = std::move(work), this]() mutable {
          Fiber* absl_nonnull previous = current_fiber;
          current_fiber = owner;
          std::move(work)();
          current_fiber = previous;
          finished.store(true, std::memory_order_release);
        }) {}

  std::thread::id thread_id;
  std::atomic<bool> cancellation_sent{false};
  PermanentEvent cancellation;
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
  if (!impl_->cancellation_sent.exchange(true, std::memory_order_acq_rel)) {
    impl_->cancellation.Notify();
  }
}

bool Fiber::Cancelled() const noexcept {
  return impl_->cancellation.HasBeenNotified();
}

Case Fiber::OnCancel() const {
  return impl_->cancellation.OnEvent();
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

Fiber* absl_nullable Fiber::Current() noexcept {
  return current_fiber;
}

bool Cancelled() {
  Fiber* absl_nullable fiber = Fiber::Current();
  return fiber != nullptr && fiber->Cancelled();
}

Case OnCancel() {
  Fiber* absl_nullable fiber = Fiber::Current();
  return fiber == nullptr ? NonSelectableCase() : fiber->OnCancel();
}

}  // namespace thread
