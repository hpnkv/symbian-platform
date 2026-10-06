// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_CONCURRENCY_EVENT_EXECUTOR_H_
#define SYMBIAN_CONCURRENCY_EVENT_EXECUTOR_H_

#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>

#include <absl/base/nullability.h>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "symbian/concurrency/event_mailbox.h"
#include "symbian/concurrency/property_watch.h"
#include "symbian/concurrency/timer_pump.h"
#include "symbian/concurrency/worker_executor.h"
#include "thread/fiber.h"

namespace symbian::concurrency {

// The event thread's sole native request-semaphore consumer. Window Server
// statuses remain owned by the caller and must be inspected on every turn.
// Native adapters remove completed requests before invoking inline callbacks.
class EventExecutor {
 public:
  EventExecutor() : scheduler_(&policy_) {}

  EventExecutor(const EventExecutor&) = delete;
  EventExecutor& operator=(const EventExecutor&) = delete;

  ~EventExecutor() { Close(); }

  absl::Status Open() {
    if (opened_ || closed_) {
      return absl::FailedPreconditionError("Event executor already opened");
    }
    const int result = timers_.Open();
    if (result != 0) {
      return symbian::StatusFromNativeError(result, "Event executor wake");
    }
    auto wake = timers_.WakeCallback();
    policy_.SetWake(wake);
    auto mailbox = std::make_shared<EventMailbox>(std::move(wake));
    {
      std::lock_guard lock(dispatch_mu_);
      mailbox_ = std::move(mailbox);
      opened_ = true;
    }
    return absl::OkStatus();
  }

  // Explicit event affinity. A11 Post/PostAt retain their worker-pool meaning.
  absl::Status DispatchToEvent(std::function<void()> callback) {
    std::shared_ptr<EventMailbox> mailbox;
    {
      std::lock_guard lock(dispatch_mu_);
      if (!opened_ || closed_) {
        return absl::FailedPreconditionError("Event executor is closed");
      }
      mailbox = mailbox_;
    }
    return mailbox->Enqueue(std::move(callback));
  }

  Task ScheduleAfter(absl::Duration delay) {
    return timers_.ScheduleAfter(delay);
  }

  Task ScheduleAt(absl::Time deadline) { return timers_.ScheduleAt(deadline); }

  absl::Status OpenProperty(int category, unsigned int key) {
    if (!opened_ || closed_ || property_) {
      return absl::FailedPreconditionError("Property owner unavailable");
    }
    auto property = std::make_unique<PropertyWatch>(timers_.WakeCallback());
    absl::Status status = property->Open(category, key);
    if (status.ok()) {
      property_ = std::move(property);
    }
    return status;
  }

  Future<int> NextProperty() {
    if (!property_ || closed_) {
      return FailedFuture<int>(
          absl::FailedPreconditionError("Property owner is closed"));
    }
    return property_->Next();
  }

  absl::Status SetProperty(int value) {
    if (!property_ || closed_) {
      return absl::FailedPreconditionError("Property owner is closed");
    }
    return property_->Set(value);
  }

  thread::Scheduler& fibers() { return scheduler_; }

  // Lazily create one shared worker for explicit compute placement. The
  // event thread calls this during setup; ordinary DispatchReady turns never
  // create an OS thread or offload callbacks implicitly.
  absl::StatusOr<WorkerExecutor* absl_nonnull> workers() {
    std::lock_guard lock(dispatch_mu_);
    if (!opened_ || closed_) {
      return absl::FailedPreconditionError("Event executor is closed");
    }
    if (!worker_) {
      worker_ = std::make_unique<WorkerExecutor>();
    }
    return worker_.get();
  }

  // Bound each source independently. Call again when HasReady() is true;
  // native and mailbox adapters resignal when a turn leaves work behind.
  absl::Status DispatchReady(std::size_t budget = 64) {
    if (!opened_ || closed_) {
      return absl::FailedPreconditionError("Event executor is closed");
    }
    if (budget == 0) {
      return absl::OkStatus();
    }
    if (property_) {
      property_->DispatchReady();
    }
    timers_.DispatchReady(budget);
    mailbox_->DispatchReady(budget);
    absl::Status status = scheduler_.RunReady(budget);
    if (!status.ok()) {
      return status;
    }
    return RearmFiberDeadline();
  }

  bool HasReady() const {
    return opened_ && !closed_ &&
           (scheduler_.HasReady() || mailbox_->Pending() != 0 ||
            (fiber_alarm_.valid() && fiber_alarm_.IsReady()));
  }

  // The caller checks Window Server statuses before this call. The request
  // semaphore preserves completion signals arriving after that check.
  void Park() const {
    if (opened_ && !closed_ && !HasReady()) {
      timers_.Park();
    }
  }

  void Close() {
    {
      std::lock_guard lock(dispatch_mu_);
      if (closed_) {
        return;
      }
      closed_ = true;
    }
    if (property_) {
      property_->Close();
      property_.reset();
    }
    if (worker_) {
      worker_->Close();
    }
    if (fiber_alarm_.valid()) {
      fiber_alarm_.Cancel();
    }
    timers_.Close();
    if (mailbox_) {
      mailbox_->Close();
      mailbox_.reset();
    }
    policy_.SetWake({});
  }

 private:
  class WakePolicy final : public thread::SchedulerPolicy {
   public:
    std::size_t PickNext(
        std::span<thread::Fiber* absl_nonnull const>) override {
      return 0;
    }

    void NotifyReady() noexcept override {
      std::function<void()> callback;
      {
        std::lock_guard lock(mu_);
        callback = wake_;
      }
      if (callback) {
        callback();
      }
    }

    void SetWake(std::function<void()> wake) {
      std::lock_guard lock(mu_);
      wake_ = std::move(wake);
    }

   private:
    std::mutex mu_;
    std::function<void()> wake_;
  };

  absl::Status RearmFiberDeadline() {
    const auto next = scheduler_.NextDeadline();
    if (next == fiber_deadline_ &&
        (!fiber_alarm_.valid() || !fiber_alarm_.IsReady())) {
      return absl::OkStatus();
    }
    if (fiber_alarm_.valid() && !fiber_alarm_.IsReady()) {
      fiber_alarm_.Cancel();
    }
    fiber_alarm_ = {};
    fiber_deadline_ = next;
    if (next == std::chrono::steady_clock::time_point::max()) {
      return absl::OkStatus();
    }
    const auto now = std::chrono::steady_clock::now();
    const auto delay =
        next <= now ? std::chrono::nanoseconds::zero() : next - now;
    fiber_alarm_ = timers_.ScheduleAfter(absl::Nanoseconds(delay.count()));
    if (auto result = fiber_alarm_.ResultIfReady(); result && !result->ok()) {
      return result->status();
    }
    return absl::OkStatus();
  }

  TimerPump timers_;
  WakePolicy policy_;
  thread::Scheduler scheduler_;
  std::mutex dispatch_mu_;
  std::shared_ptr<EventMailbox> mailbox_;
  std::unique_ptr<PropertyWatch> property_;
  std::unique_ptr<WorkerExecutor> worker_;
  Task fiber_alarm_;
  std::chrono::steady_clock::time_point fiber_deadline_ =
      std::chrono::steady_clock::time_point::max();
  bool opened_ = false;
  bool closed_ = false;
};

}  // namespace symbian::concurrency

#endif  // SYMBIAN_CONCURRENCY_EVENT_EXECUTOR_H_
