// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.
// Native completion adapter for the A11-derived stackless Future profile.

#ifndef SYMBIAN_CONCURRENCY_TIMER_PUMP_H_
#define SYMBIAN_CONCURRENCY_TIMER_PUMP_H_

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "absl/time/time.h"
#include "symbian/concurrency/future.h"
#include "symbian/concurrency/native_timer.h"
#include "symbian/native_status.h"

namespace symbian::concurrency {

namespace internal {
inline int TimerSliceMicroseconds(absl::Duration remaining) {
  const absl::Duration max_slice =
      absl::Microseconds(std::numeric_limits<std::int32_t>::max());
  const absl::Duration slice = remaining < max_slice ? remaining : max_slice;
  return static_cast<int>(
      absl::ToInt64Microseconds(absl::Ceil(slice, absl::Microseconds(1))));
}

inline absl::StatusOr<absl::Duration> RemainingAtRegistration(
    absl::Time deadline, absl::Time now) {
  if (deadline == absl::InfiniteFuture()) {
    return absl::InfiniteDuration();
  }
  if (deadline <= now) {
    return absl::ZeroDuration();
  }
  const absl::Duration remaining = deadline - now;
  if (remaining == absl::InfiniteDuration()) {
    return absl::OutOfRangeError("Absolute deadline is too distant");
  }
  return remaining;
}
}  // namespace internal

// One OS event thread owns this pump and every timer in it. The caller also
// owns Window Server statuses: inspect and dispatch all ready statuses before
// Park(), then inspect them again after Park() returns. Park is the only wait
// in the combined loop; no timer worker or second request consumer is created.
class TimerPump {
 public:
  static constexpr std::size_t kDefaultMaxPending = 64;
  using WallClock = absl::Time (*)();

  explicit TimerPump(std::size_t max_pending = kDefaultMaxPending,
                     WallClock wall_now = &absl::Now)
      : max_pending_(max_pending), wall_now_(wall_now) {}

  TimerPump(const TimerPump&) = delete;
  TimerPump& operator=(const TimerPump&) = delete;

  ~TimerPump() { Close(); }

  int Open() {
    if (closed_ || wake_) {
      return -11;  // KErrAlreadyExists.
    }
    auto wake = std::make_shared<WakeTarget>();
    const int result = wake->Open();
    if (result == 0) {
      wake_ = std::move(wake);
    }
    return result;
  }

  // Another native request owner can share this event thread's one wait.
  // The callback stays safe after Close and never consumes the semaphore.
  std::function<void()> WakeCallback() const {
    return [target = std::weak_ptr<WakeTarget>(wake_)] {
      if (auto owned = target.lock()) {
        owned->Notify();
      }
    };
  }

  // Absolute times are wall times. Convert once at acceptance; later device
  // clock corrections do not move an already accepted timer.
  Task ScheduleAt(absl::Time deadline) {
    if (wall_now_ == nullptr) {
      return FailedTask(absl::FailedPreconditionError("No wall clock"));
    }
    auto remaining = internal::RemainingAtRegistration(deadline, wall_now_());
    if (!remaining.ok()) {
      return FailedTask(remaining.status());
    }
    return ScheduleAfter(*remaining);
  }

  Task ScheduleAfter(absl::Duration delay) {
    if (closed_ || !wake_) {
      return FailedTask(absl::FailedPreconditionError("Timer pump is closed"));
    }
    if (delay < absl::ZeroDuration()) {
      return FailedTask(
          absl::InvalidArgumentError("Timer delay must be nonnegative"));
    }
    if (entries_.size() >= max_pending_) {
      return FailedTask(absl::ResourceExhaustedError("Timer pump is full"));
    }
    auto entry = std::make_shared<Entry>();
    entry->remaining = delay;
    if (delay != absl::InfiniteDuration()) {
      const int opened = entry->timer.Open();
      if (opened != 0) {
        return FailedTask(
            symbian::StatusFromNativeError(opened, "RTimer create"));
      }
    }
    Task task = entry->promise.future();
    entry->promise.SetCancellationCallback(
        [entry = std::weak_ptr<Entry>(entry),
         wake = std::weak_ptr<WakeTarget>(wake_)] {
          if (auto owned = entry.lock()) {
            owned->cancel_requested.store(1, std::memory_order_release);
            if (auto target = wake.lock()) {
              target->Notify();
            }
          }
        });
    // Ownership is visible to DispatchReady before an immediate native
    // completion can occur. OnReady never runs until the entry is removed.
    entries_.push_back(entry);
    if (delay != absl::InfiniteDuration()) {
      const int started = ArmNext(*entry);
      if (started != 0) {
        entries_.pop_back();
        entry->timer.Close();
        entry->promise.SetError(
            symbian::StatusFromNativeError(started, "RTimer arm"));
      }
    }
    return task;
  }

  // Only the event thread calls DispatchReady. A11 OnReady callbacks may run
  // inline, so remove finished entries before publishing their results.
  std::size_t DispatchReady(std::size_t budget = 64) {
    if (closed_ || !wake_ || budget == 0) {
      return 0;
    }
    wake_->Clear();
    std::vector<std::pair<std::shared_ptr<Entry>, int>> completed;
    for (auto it = entries_.begin(); it != entries_.end();) {
      auto& entry = *it;
      if (completed.size() == budget) {
        wake_->Notify();
        break;
      }
      if (entry->remaining == absl::InfiniteDuration()) {
        if (entry->cancel_requested.load(std::memory_order_acquire) == 0) {
          ++it;
          continue;
        }
        completed.emplace_back(entry, symbian::native_error::kCancel);
        it = entries_.erase(it);
        continue;
      }
      if (!entry->cancel_submitted &&
          entry->cancel_requested.load(std::memory_order_acquire) != 0) {
        entry->timer.Cancel();
        entry->cancel_submitted = true;
      }
      if (!entry->timer.IsReady()) {
        ++it;
        continue;
      }
      int code = entry->timer.Result();
      if (code == 0) {
        const auto elapsed =
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - entry->armed_at);
        entry->remaining -= absl::Nanoseconds(elapsed.count());
        if (entry->remaining > absl::ZeroDuration()) {
          if (entry->cancel_requested.load(std::memory_order_acquire) != 0) {
            code = symbian::native_error::kCancel;
          } else {
            const int rearmed = ArmNext(*entry);
            if (rearmed == 0) {
              ++it;
              continue;
            }
            code = rearmed;
          }
        }
      }
      entry->timer.Close();
      completed.emplace_back(entry, code);
      it = entries_.erase(it);
    }
    for (auto& [entry, code] : completed) {
      if (code == 0) {
        entry->promise.SetValue(Unit{});
      } else if (code == -3) {  // KErrCancel.
        entry->promise.SetError(absl::CancelledError("Timer cancelled"));
      } else {
        entry->promise.SetError(
            symbian::StatusFromNativeError(code, "RTimer completion"));
      }
    }
    return completed.size();
  }

  // The caller is the sole consumer of this OS thread's request semaphore.
  // Check Window Server statuses and call DispatchReady before parking.
  void Park() const {
    if (!closed_ && wake_) {
      SymbianRuntimeWaitForAnyRequest();
    }
  }

  void Close() {
    if (closed_) {
      return;
    }
    closed_ = true;
    if (wake_) {
      wake_->Close();
    }
    auto entries = std::move(entries_);
    for (auto& entry : entries) {
      entry->timer.Close();
      entry->promise.SetError(absl::CancelledError("Timer pump closed"));
    }
    wake_.reset();
  }

 private:
  struct WakeTarget {
    int Open() { return SymbianRuntimeWakeCreate(&native_); }

    void Notify() {
      thread::MutexLock lock(&mu_);
      if (native_ && !pending_) {
        pending_ = true;
        SymbianRuntimeWakeSignal(native_);
      }
    }

    void Clear() {
      thread::MutexLock lock(&mu_);
      pending_ = false;
    }

    void Close() {
      thread::MutexLock lock(&mu_);
      if (native_) {
        SymbianRuntimeWakeClose(native_);
        native_ = nullptr;
      }
    }

    thread::Mutex mu_;
    SymbianRuntimeWakeState* native_ = nullptr;
    bool pending_ = false;
  };

  struct Entry {
    NativeTimer timer;
    Promise<Unit> promise;
    std::atomic<int> cancel_requested{0};
    bool cancel_submitted = false;
    absl::Duration remaining = absl::ZeroDuration();
    std::chrono::steady_clock::time_point armed_at;
  };

  static int ArmNext(Entry& entry) {
    entry.armed_at = std::chrono::steady_clock::now();
    return entry.timer.Start(internal::TimerSliceMicroseconds(entry.remaining));
  }

  std::shared_ptr<WakeTarget> wake_;
  std::vector<std::shared_ptr<Entry>> entries_;
  std::size_t max_pending_;
  WallClock wall_now_;
  bool closed_ = false;
};

}  // namespace symbian::concurrency

#endif  // SYMBIAN_CONCURRENCY_TIMER_PUMP_H_
