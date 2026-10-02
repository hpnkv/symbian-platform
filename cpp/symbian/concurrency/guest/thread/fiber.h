// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_GUEST_THREAD_FIBER_H_
#define SYMBIAN_GUEST_THREAD_FIBER_H_

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <span>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/time/time.h"

namespace thread {

class Fiber;
class Mutex;
class CondVar;
void FiberEntry();

// Application-provided ordering and event-loop wake integration. Both hooks
// run outside Scheduler's internal lock. NotifyReady may run on a worker OS
// thread, so implementations must coalesce and dispatch to the event thread.
class SchedulerPolicy {
 public:
  virtual ~SchedulerPolicy() = default;
  virtual std::size_t PickNext(std::span<Fiber* const> ready) = 0;
  virtual void NotifyReady() noexcept = 0;
};

// One explicitly pumped executor, pinned to its creating OS thread. RunReady
// never consumes the native request semaphore; the event owner calls it after
// dispatching native completions and when its next fiber deadline expires.
class Scheduler {
 public:
  explicit Scheduler(SchedulerPolicy* policy = nullptr);
  Scheduler(const Scheduler&) = delete;
  Scheduler& operator=(const Scheduler&) = delete;
  ~Scheduler();

  absl::Status RunReady(std::size_t max_turns);
  bool HasReady() const;
  std::chrono::steady_clock::time_point NextDeadline() const;
  static Scheduler* Current() noexcept;

 private:
  friend class Fiber;
  friend class Mutex;
  friend class CondVar;
  friend void FiberEntry();
  void Add(Fiber* fiber);
  void Remove(Fiber* fiber);
  void Wake(Fiber* fiber);
  bool WakeWithoutNotify(Fiber* fiber);
  void NotifyReady() noexcept;
  void PreparePark(Fiber* fiber,
                   std::chrono::steady_clock::time_point deadline);
  void CancelPark(Fiber* fiber);
  void Suspend(Fiber* fiber);

  const std::thread::id owner_;
  SchedulerPolicy* const policy_;
  mutable std::mutex mu_;
  std::deque<Fiber*> ready_;
  std::vector<Fiber*> fibers_;
  Fiber* current_ = nullptr;
  std::uintptr_t root_sp_ = 0;
};

class Fiber {
 public:
  using Work = absl::AnyInvocable<void() &&>;
  static constexpr std::size_t kDefaultStackBytes = 16 * 1024;

  Fiber(Scheduler& scheduler, Work work,
        std::size_t stack_bytes = kDefaultStackBytes);

  template <typename F>
  requires(std::is_invocable_r_v<void, std::decay_t<F>> &&
           !std::is_same_v<std::decay_t<F>, Work>)
      Fiber(Scheduler& scheduler, F&& work,
            std::size_t stack_bytes = kDefaultStackBytes)
      : Fiber(scheduler, Work(std::forward<F>(work)), stack_bytes) {}

  Fiber(const Fiber&) = delete;
  Fiber& operator=(const Fiber&) = delete;
  ~Fiber();

  bool Finished() const noexcept {
    std::lock_guard lock(scheduler_.mu_);
    return finished_;
  }

  static Fiber* Current() noexcept;
  static void Yield();
  static void SleepFor(absl::Duration duration);

 private:
  friend class Scheduler;
  friend class Mutex;
  friend class CondVar;
  friend void FiberEntry();
  Scheduler& scheduler_;
  Work work_;
  std::unique_ptr<std::uintptr_t[]> stack_;
  std::uintptr_t stack_sp_ = 0;
  bool queued_ = false;   // guarded by Scheduler::mu_
  bool waiting_ = false;  // guarded by Scheduler::mu_
  bool finished_ = false;
  std::chrono::steady_clock::time_point deadline_ =
      std::chrono::steady_clock::time_point::max();
};

}  // namespace thread

#endif  // SYMBIAN_GUEST_THREAD_FIBER_H_
