// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "thread/fiber.h"

#include <algorithm>
#include <cstdlib>

#include <pthread.h>

#include "thread/selectables.h"

extern "C" void SymbianFiberSwap(std::uintptr_t* saved_sp,
                                 std::uintptr_t next_sp);

namespace thread {
namespace {
pthread_key_t scheduler_key;
pthread_once_t scheduler_key_once = PTHREAD_ONCE_INIT;

void CreateSchedulerKey() {
  if (pthread_key_create(&scheduler_key, nullptr) != 0) {
    std::abort();
  }
}

std::chrono::steady_clock::time_point DeadlineAfter(absl::Duration duration) {
  if (duration == absl::InfiniteDuration()) {
    return std::chrono::steady_clock::time_point::max();
  }
  const auto now = std::chrono::steady_clock::now();
  if (duration <= absl::ZeroDuration()) {
    return now;
  }
  const absl::Duration safe =
      duration < absl::Hours(24 * 365) ? duration : absl::Hours(24 * 365);
  return now + std::chrono::nanoseconds(absl::ToInt64Nanoseconds(safe));
}
}  // namespace

Scheduler::Scheduler(SchedulerPolicy* policy)
    : owner_(std::this_thread::get_id()), policy_(policy) {
  pthread_once(&scheduler_key_once, CreateSchedulerKey);
}

Scheduler::~Scheduler() {
  if (std::this_thread::get_id() != owner_) {
    std::abort();
  }
  std::lock_guard lock(mu_);
  if (!fibers_.empty() || current_ != nullptr) {
    std::abort();
  }
}

Scheduler* Scheduler::Current() noexcept {
  pthread_once(&scheduler_key_once, CreateSchedulerKey);
  return static_cast<Scheduler*>(pthread_getspecific(scheduler_key));
}

void Scheduler::Add(Fiber* fiber) {
  {
    std::lock_guard lock(mu_);
    fibers_.push_back(fiber);
    ready_.push_back(fiber);
    fiber->queued_ = true;
  }
  if (policy_ != nullptr) {
    policy_->NotifyReady();
  }
}

void Scheduler::Remove(Fiber* fiber) {
  if (std::this_thread::get_id() != owner_) {
    std::abort();
  }
  std::lock_guard lock(mu_);
  if (current_ == fiber || fiber->queued_ || fiber->waiting_ ||
      !fiber->finished_) {
    std::abort();
  }
  auto it = std::find(fibers_.begin(), fibers_.end(), fiber);
  if (it == fibers_.end()) {
    std::abort();
  }
  fibers_.erase(it);
}

void Scheduler::Wake(Fiber* fiber) {
  if (WakeWithoutNotify(fiber)) {
    NotifyReady();
  }
}

bool Scheduler::WakeWithoutNotify(Fiber* fiber) {
  bool notify = false;
  {
    std::lock_guard lock(mu_);
    if (!fiber->waiting_ || fiber->finished_) {
      return false;
    }
    fiber->waiting_ = false;
    fiber->deadline_ = std::chrono::steady_clock::time_point::max();
    if (!fiber->queued_) {
      ready_.push_back(fiber);
      fiber->queued_ = true;
      notify = true;
    }
  }
  return notify;
}

void Scheduler::NotifyReady() noexcept {
  if (policy_ != nullptr) {
    policy_->NotifyReady();
  }
}

void Scheduler::PreparePark(Fiber* fiber,
                            std::chrono::steady_clock::time_point deadline) {
  std::lock_guard lock(mu_);
  if (current_ != fiber || fiber->waiting_) {
    std::abort();
  }
  fiber->waiting_ = true;
  fiber->deadline_ = deadline;
}

void Scheduler::CancelPark(Fiber* fiber) {
  std::lock_guard lock(mu_);
  fiber->waiting_ = false;
  fiber->deadline_ = std::chrono::steady_clock::time_point::max();
}

void Scheduler::Suspend(Fiber* fiber) {
  if (current_ != fiber) {
    std::abort();
  }
  SymbianFiberSwap(&fiber->stack_sp_, root_sp_);
}

bool Scheduler::HasReady() const {
  std::lock_guard lock(mu_);
  if (!ready_.empty()) {
    return true;
  }
  const auto now = std::chrono::steady_clock::now();
  for (Fiber* fiber : fibers_) {
    if (fiber->waiting_ && fiber->deadline_ <= now) {
      return true;
    }
  }
  return false;
}

std::chrono::steady_clock::time_point Scheduler::NextDeadline() const {
  std::lock_guard lock(mu_);
  auto next = std::chrono::steady_clock::time_point::max();
  for (Fiber* fiber : fibers_) {
    if (fiber->waiting_) {
      next = std::min(next, fiber->deadline_);
    }
  }
  return next;
}

absl::Status Scheduler::RunReady(std::size_t max_turns) {
  if (std::this_thread::get_id() != owner_) {
    return absl::FailedPreconditionError(
        "fiber scheduler belongs to another OS thread");
  }
  if (Current() != nullptr) {
    return absl::FailedPreconditionError("nested fiber scheduler pump");
  }
  if (pthread_setspecific(scheduler_key, this) != 0) {
    return absl::InternalError("cannot bind guest fiber scheduler TLS");
  }
  // Reuse the ready snapshot across a turn. Policy selection still happens
  // outside mu, but a yielding fiber does not allocate a new vector each time.
  std::vector<Fiber*> snapshot;
  for (std::size_t turn = 0; turn < max_turns; ++turn) {
    Fiber* next = nullptr;
    snapshot.clear();
    {
      std::lock_guard lock(mu_);
      const auto now = std::chrono::steady_clock::now();
      for (Fiber* fiber : fibers_) {
        if (fiber->waiting_ && fiber->deadline_ <= now) {
          fiber->waiting_ = false;
          fiber->deadline_ = std::chrono::steady_clock::time_point::max();
          if (!fiber->queued_) {
            ready_.push_back(fiber);
            fiber->queued_ = true;
          }
        }
      }
      if (!ready_.empty()) {
        snapshot.assign(ready_.begin(), ready_.end());
      }
    }
    if (snapshot.empty()) {
      break;
    }
    const std::size_t index =
        policy_ == nullptr ? 0 : policy_->PickNext(snapshot);
    if (index >= snapshot.size()) {
      pthread_setspecific(scheduler_key, nullptr);
      return absl::InvalidArgumentError(
          "fiber scheduler selected invalid ready index");
    }
    {
      std::lock_guard lock(mu_);
      next = snapshot[index];
      auto it = std::find(ready_.begin(), ready_.end(), next);
      if (it == ready_.end()) {
        std::abort();
      }
      ready_.erase(it);
      next->queued_ = false;
      current_ = next;
    }
    SymbianFiberSwap(&root_sp_, next->stack_sp_);
    current_ = nullptr;
  }
  pthread_setspecific(scheduler_key, nullptr);
  return absl::OkStatus();
}

void FiberEntry() {
  Scheduler* scheduler = Scheduler::Current();
  if (scheduler == nullptr || scheduler->current_ == nullptr) {
    std::abort();
  }
  Fiber* fiber = scheduler->current_;
  std::move(fiber->work_)();
  {
    std::lock_guard lock(scheduler->mu_);
    fiber->finished_ = true;
  }
  SymbianFiberSwap(&fiber->stack_sp_, scheduler->root_sp_);
  std::abort();
}

Fiber::Fiber(Scheduler& scheduler, Work work, std::size_t stack_bytes)
    : scheduler_(scheduler),
      work_(std::move(work)),
      cancellation_(std::make_unique<PermanentEvent>()) {
  if (std::this_thread::get_id() != scheduler.owner_ || stack_bytes < 4096 ||
      stack_bytes > 1024 * 1024 || stack_bytes % sizeof(std::uintptr_t) != 0 ||
      !work_) {
    std::abort();
  }
  const std::size_t words = stack_bytes / sizeof(std::uintptr_t);
  stack_ = std::make_unique<std::uintptr_t[]>(words);
  std::uintptr_t top = reinterpret_cast<std::uintptr_t>(stack_.get() + words);
  top &= ~std::uintptr_t{7};
  auto* frame = reinterpret_cast<std::uintptr_t*>(top) - 9;
  for (int index = 0; index < 8; ++index) {
    frame[index] = 0;
  }
  frame[8] = reinterpret_cast<std::uintptr_t>(&FiberEntry);
  stack_sp_ = reinterpret_cast<std::uintptr_t>(frame);
  scheduler_.Add(this);
}

Fiber::~Fiber() {
  scheduler_.Remove(this);
}

Fiber* Fiber::Current() noexcept {
  Scheduler* scheduler = Scheduler::Current();
  return scheduler == nullptr ? nullptr : scheduler->current_;
}

void Fiber::Cancel() {
  if (!cancel_requested_.exchange(true, std::memory_order_acq_rel)) {
    cancellation_->Notify();
  }
}

bool Fiber::Cancelled() const noexcept {
  return cancellation_->HasBeenNotified();
}

Case Fiber::OnCancel() const {
  return cancellation_->OnEvent();
}

bool Cancelled() {
  Fiber* current = Fiber::Current();
  return current != nullptr && current->Cancelled();
}

Case OnCancel() {
  Fiber* current = Fiber::Current();
  return current == nullptr ? NonSelectableCase() : current->OnCancel();
}

void Fiber::Yield() {
  Fiber* fiber = Current();
  if (fiber == nullptr) {
    std::this_thread::yield();
    return;
  }
  fiber->scheduler_.PreparePark(fiber,
                                std::chrono::steady_clock::time_point::max());
  fiber->scheduler_.Wake(fiber);
  fiber->scheduler_.Suspend(fiber);
}

void Fiber::SleepFor(absl::Duration duration) {
  Fiber* fiber = Current();
  if (fiber == nullptr) {
    std::abort();
  }
  if (duration <= absl::ZeroDuration()) {
    Yield();
    return;
  }
  while (duration > absl::ZeroDuration()) {
    const auto start = std::chrono::steady_clock::now();
    fiber->scheduler_.PreparePark(fiber, DeadlineAfter(duration));
    fiber->scheduler_.Suspend(fiber);
    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - start);
    duration -= absl::Nanoseconds(elapsed.count());
  }
}

}  // namespace thread
