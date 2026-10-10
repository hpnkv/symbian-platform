// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_GUEST_THREAD_FIBER_H_
#define SYMBIAN_GUEST_THREAD_FIBER_H_

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include <absl/base/nullability.h>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/time/time.h"

namespace thread {

class Fiber;
class Mutex;
class CondVar;
class PermanentEvent;
struct Case;
void FiberEntry();

// Application-provided ordering and event-loop wake integration. Both hooks
// run outside Scheduler's internal lock. NotifyReady may run on a worker OS
// thread, so implementations must coalesce and dispatch to the event thread.
struct SchedulerPolicy {
  std::function<std::size_t(std::span<Fiber* absl_nonnull const>)> pick_next;
  std::function<void()> notify_ready;

  std::size_t PickNext(std::span<Fiber* absl_nonnull const> ready) const {
    return pick_next(ready);
  }

  void NotifyReady() const noexcept { notify_ready(); }
};

// One explicitly pumped executor, pinned to its creating OS thread. RunReady
// never consumes the native request semaphore; the event owner calls it after
// dispatching native completions and when its next fiber deadline expires.
class Scheduler {
 public:
  explicit Scheduler(SchedulerPolicy* absl_nullable policy = nullptr);
  Scheduler(const Scheduler&) = delete;
  Scheduler& operator=(const Scheduler&) = delete;
  ~Scheduler();

  absl::Status RunReady(std::size_t max_turns);
  bool HasReady() const;
  std::chrono::steady_clock::time_point NextDeadline() const;
  static Scheduler* absl_nullable Current() noexcept;

 private:
  friend class Fiber;
  friend class Mutex;
  friend class CondVar;
  friend void FiberEntry();
  void Add(Fiber* absl_nonnull fiber);
  void Remove(Fiber* absl_nullable fiber);
  void Wake(Fiber* absl_nonnull fiber);
  bool WakeWithoutNotify(Fiber* absl_nullable fiber);
  void NotifyReady() noexcept;
  void PreparePark(Fiber* absl_nonnull fiber,
                   std::chrono::steady_clock::time_point deadline);
  void CancelPark(Fiber* absl_nonnull fiber);
  void Suspend(Fiber* absl_nonnull fiber);

  const std::thread::id owner_;
  SchedulerPolicy* absl_nullable const policy_;
  mutable std::mutex mu_;
  std::deque<Fiber* absl_nonnull> ready_;
  std::vector<Fiber* absl_nonnull> fibers_;
  Fiber* absl_nullable current_ = nullptr;
  std::uintptr_t root_sp_ = 0;
};

class Fiber {
 public:
  using Work = absl::AnyInvocable<void() &&>;
  static constexpr std::size_t kDefaultStackBytes = 16 * 1024;

  Fiber(Scheduler* absl_nonnull scheduler, Work work,
        std::size_t stack_bytes = kDefaultStackBytes);

  template <typename F>
  requires(std::is_invocable_r_v<void, std::decay_t<F>> &&
           !std::is_same_v<std::decay_t<F>, Work>)
      Fiber(Scheduler* absl_nonnull scheduler, F&& work,
            std::size_t stack_bytes = kDefaultStackBytes)
      : Fiber(scheduler, Work(std::forward<F>(work)), stack_bytes) {}

  Fiber(const Fiber&) = delete;
  Fiber& operator=(const Fiber&) = delete;
  ~Fiber();

  bool Finished() const noexcept {
    std::lock_guard lock(scheduler_.mu_);
    return finished_;
  }

  void Cancel();
  bool Cancelled() const noexcept;
  Case OnCancel() const;

  static Fiber* absl_nullable Current() noexcept;
  static void Yield();
  static void SleepFor(absl::Duration duration);

 private:
  friend class Scheduler;
  friend class Mutex;
  friend class CondVar;
  friend void FiberEntry();
  Scheduler& scheduler_;
  Work work_;
  std::unique_ptr<PermanentEvent> cancellation_;
  std::atomic<bool> cancel_requested_{false};
  std::unique_ptr<std::uintptr_t[]> stack_;
  std::uintptr_t stack_sp_ = 0;
  bool queued_ = false;   // guarded by Scheduler::mu_
  bool waiting_ = false;  // guarded by Scheduler::mu_
  bool finished_ = false;
  std::chrono::steady_clock::time_point deadline_ =
      std::chrono::steady_clock::time_point::max();
};

bool Cancelled();
Case OnCancel();

}  // namespace thread

#endif  // SYMBIAN_GUEST_THREAD_FIBER_H_
