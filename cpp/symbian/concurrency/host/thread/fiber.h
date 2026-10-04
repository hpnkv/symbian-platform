// Copyright 2026 The Action Engine Authors and the Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0 (the "License");
// http://www.apache.org/licenses/LICENSE-2.0
//
// Bounded host adaptation of A11 cpp/thread/thread/fiber.h at
// fcccb8cb6e1e67d7ba0822ac14cece9ee4c7091b. The callable runs on a
// Boost-backed fiber pinned to its creating OS thread. This profile does not
// implement A11 trees, Select, pool scheduling or forced-unwind cancellation.

#ifndef THREAD_FIBER_H_
#define THREAD_FIBER_H_

#include <memory>
#include <type_traits>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"

namespace thread {

struct Case;

class Fiber {
 public:
  using Work = absl::AnyInvocable<void() &&>;

  explicit Fiber(Work work);

  template <typename F>
  requires(std::is_invocable_r_v<void, std::decay_t<F>> &&
           !std::is_same_v<std::decay_t<F>, Work>) explicit Fiber(F&& work)
      : Fiber(Work(std::forward<F>(work))) {}

  Fiber(const Fiber&) = delete;
  Fiber& operator=(const Fiber&) = delete;
  Fiber(Fiber&&) = delete;
  Fiber& operator=(Fiber&&) = delete;

  // Join is required before destruction. No forced unwind or detach occurs.
  ~Fiber();

  // Cancellation is cooperative: work can inspect Current()->Cancelled().
  void Cancel() noexcept;
  bool Cancelled() const noexcept;
  Case OnCancel() const;
  bool Finished() const noexcept;

  // Must be called on the creating OS thread. A wrong-thread join returns
  // FailedPrecondition without disturbing ownership; a second join does too.
  absl::Status Join();

  // Null outside an SDK-owned fiber. This differs from A11's root placeholder.
  static Fiber* Current() noexcept;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace thread

namespace thread {
bool Cancelled();
Case OnCancel();
}  // namespace thread

#endif  // THREAD_FIBER_H_
