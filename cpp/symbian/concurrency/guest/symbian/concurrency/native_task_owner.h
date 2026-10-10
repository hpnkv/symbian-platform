// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_CONCURRENCY_NATIVE_TASK_OWNER_H_
#define SYMBIAN_CONCURRENCY_NATIVE_TASK_OWNER_H_

#include <atomic>
#include <memory>
#include <optional>

#include <absl/base/nullability.h>
#include <absl/status/status_macros.h>

#include "absl/time/time.h"
#include "symbian/concurrency/event_executor.h"
#include "symbian/concurrency/task_group.h"

namespace symbian::concurrency {

// Starts one timer and one real property subscription under a shared owner.
// Close requests cancellation; Finish settles only after both native adapters
// have published their drained results. Call Start and Close on the event
// thread. A worker may cancel the returned Task.
class NativeTaskOwner {
 public:
  explicit NativeTaskOwner(EventExecutor* absl_nonnull executor)
      : executor_(*executor) {}

  NativeTaskOwner(const NativeTaskOwner&) = delete;
  NativeTaskOwner& operator=(const NativeTaskOwner&) = delete;

  ~NativeTaskOwner() { Close(); }

  Task Start(absl::Duration timer_delay,
             absl::Time deadline = absl::InfiniteFuture()) {
    if (closed_ || state_) {
      return FailedTask(absl::FailedPreconditionError(
          "Native task owner already started or closed"));
    }
    if (deadline != absl::InfiniteFuture() && deadline <= absl::Now()) {
      return FailedTask(absl::DeadlineExceededError("Owner deadline expired"));
    }
    auto state = std::make_shared<State>();
    state_ = state;  // Establish owner state before either native submission.
    state->completion.SetCancellationCallback(
        [weak = std::weak_ptr<State>(state)] {
          if (auto owned = weak.lock()) {
            owned->RequestCancel();
          }
        });
    state->timer = executor_.ScheduleAfter(timer_delay);
    state->property = executor_.NextProperty();
    TaskGroup children;
    children.Add(state->timer);
    children.Add(
        Then(state->property,
             [](const absl::StatusOr<int>& result) -> absl::StatusOr<Unit> {
               ABSL_RETURN_IF_ERROR(result.status());
               return Unit{};
             }));
    state->children = children.Finish();
    if (deadline != absl::InfiniteFuture()) {
      state->alarm = executor_.ScheduleAt(deadline);
    }
    state->timer.OnReady([weak = std::weak_ptr<State>(state)](
                             const absl::StatusOr<Unit>& result) {
      if (!result.ok()) {
        if (auto owned = weak.lock()) {
          owned->property.Cancel();
        }
      }
    });
    state->property.OnReady([weak = std::weak_ptr<State>(state)](
                                const absl::StatusOr<int>& result) {
      if (!result.ok()) {
        if (auto owned = weak.lock()) {
          owned->timer.Cancel();
        }
      }
    });
    if (state->alarm.valid()) {
      state->alarm.OnReady([state](const absl::StatusOr<Unit>& result) {
        state->alarm_result = result;
        if (result.ok()) {
          state->timed_out = true;
          state->timer.Cancel();
          state->property.Cancel();
        } else if (result.status().code() != absl::StatusCode::kCancelled) {
          state->timer.Cancel();
          state->property.Cancel();
        }
        state->Check();
      });
    }
    state->children.OnReady([state](const absl::StatusOr<Unit>& result) {
      state->child_result = result;
      if (state->alarm.valid() && !state->alarm.IsReady()) {
        state->alarm.Cancel();
      }
      state->Check();
    });
    return state->joined;
  }

  Future<int> property_result() const {
    return state_ ? state_->property : Future<int>();
  }

  Task Finish() const {
    return state_ ? state_->joined
                  : FailedTask(absl::FailedPreconditionError(
                        "Native task owner was not started"));
  }

  void Close() {
    if (closed_) {
      return;
    }
    closed_ = true;
    if (state_) {
      state_->RequestCancel();
    }
  }

 private:
  struct State {
    void RequestCancel() {
      cancel_requested.store(true, std::memory_order_release);
      timer.Cancel();
      property.Cancel();
      alarm.Cancel();
    }

    void Check() {
      if (!child_result || (alarm.valid() && !alarm_result)) {
        return;
      }
      if (timed_out) {
        completion.SetError(
            absl::DeadlineExceededError("Owner deadline expired"));
      } else if (alarm_result && !alarm_result->ok() &&
                 alarm_result->status().code() !=
                     absl::StatusCode::kCancelled) {
        completion.SetError(alarm_result->status());
      } else if (!child_result->ok()) {
        completion.SetError(child_result->status());
      } else if (cancel_requested.load(std::memory_order_acquire)) {
        completion.SetError(absl::CancelledError("Native task owner closed"));
      } else {
        completion.SetValue(Unit{});
      }
    }

    Promise<Unit> completion;
    Task joined = completion.future();
    Task timer;
    Future<int> property;
    Task alarm;
    Task children;
    std::optional<absl::StatusOr<Unit>> child_result;
    std::optional<absl::StatusOr<Unit>> alarm_result;
    std::atomic<bool> cancel_requested{false};
    bool timed_out = false;
  };

  EventExecutor& executor_;
  std::shared_ptr<State> state_;
  bool closed_ = false;
};

}  // namespace symbian::concurrency

#endif  // SYMBIAN_CONCURRENCY_NATIVE_TASK_OWNER_H_
