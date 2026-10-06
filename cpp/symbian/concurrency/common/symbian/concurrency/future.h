// Copyright 2026 The A11 Authors.
// Licensed under the Apache License, Version 2.0 (the "License");
// http://www.apache.org/licenses/LICENSE-2.0
//
// Shared stackless adaptation of A11 cpp/a11/concurrency/future.h at
// fcccb8cb6e1e67d7ba0822ac14cece9ee4c7091b. This first stackless profile
// keeps A11's shared completion, inline OnReady/Then, cancellation request,
// and abandoned-promise semantics. Guest event threads reject an unresolved
// Await; a host with the Boost primitive backend can park its current fiber.

#ifndef SYMBIAN_CONCURRENCY_FUTURE_H_
#define SYMBIAN_CONCURRENCY_FUTURE_H_

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

#include <absl/base/nullability.h>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "thread/boost_primitives.h"

namespace symbian::concurrency {

#if defined(__SYMBIAN32__)
class EventExecutor;
#endif

struct Unit {
  friend bool operator==(Unit, Unit) = default;
};

template <typename T>
class Future;

namespace internal {
template <typename T>
struct FutureState {
  thread::Mutex mu;
  thread::CondVar ready;
  std::optional<absl::StatusOr<T>> result;
  std::function<void()> cancel;
  std::vector<std::function<void(const absl::StatusOr<T>&)>> callbacks;
};
}  // namespace internal

template <typename T>
class Promise {
 public:
  Promise() : state_(std::make_shared<internal::FutureState<T>>()) {}

  Promise(const Promise&) = delete;
  Promise& operator=(const Promise&) = delete;
  Promise(Promise&&) noexcept = default;

  Promise& operator=(Promise&& other) noexcept {
    if (this != &other) {
      Abandon();
      state_ = std::move(other.state_);
    }
    return *this;
  }

  ~Promise() { Abandon(); }

  Future<T> future() const { return Future<T>(state_); }

  bool SetCancellationCallback(std::function<void()> cancel) {
    if (!state_) {
      return false;
    }
    thread::MutexLock lock(&state_->mu);
    if (state_->result) {
      return false;
    }
    state_->cancel = std::move(cancel);
    return true;
  }

  bool SetValue(T value) {
    return SetResult(absl::StatusOr<T>(std::move(value)));
  }

  bool SetError(absl::Status error) {
    if (error.ok()) {
      return false;
    }
    return SetResult(absl::StatusOr<T>(std::move(error)));
  }

  bool SetResult(absl::StatusOr<T> result) {
    auto state = state_;
    if (!state) {
      return false;
    }
    std::vector<std::function<void(const absl::StatusOr<T>&)>> callbacks;
    {
      thread::MutexLock lock(&state->mu);
      if (state->result) {
        return false;
      }
      state->result.emplace(std::move(result));
      state->cancel = {};
      callbacks.swap(state->callbacks);
    }
    state->ready.SignalAll();
    // A11 permits inline completion. No callback runs under the state lock.
    for (auto& callback : callbacks) {
      callback(*state->result);
    }
    return true;
  }

 private:
  void Abandon() {
    if (state_) {
      SetError(absl::CancelledError("Promise was abandoned"));
      state_.reset();
    }
  }

  std::shared_ptr<internal::FutureState<T>> state_;
};

template <typename T>
class Future {
 public:
  Future() = default;

  bool valid() const { return state_ != nullptr; }

  bool IsReady() const {
    if (!state_) {
      return false;
    }
    thread::MutexLock lock(&state_->mu);
    return state_->result.has_value();
  }

  // Cancellation requests producer action; the result is published by the
  // producer. A ready result makes cancellation an idempotent success.
  bool Cancel() const {
    if (!state_) {
      return false;
    }
    std::function<void()> cancel;
    {
      thread::MutexLock lock(&state_->mu);
      if (state_->result) {
        return true;
      }
      cancel = state_->cancel;
    }
    if (!cancel) {
      return false;
    }
    cancel();
    return true;
  }

  // Stackless operations never block a guest event thread.
  std::optional<absl::StatusOr<T>> ResultIfReady() const {
    if (!state_) {
      return std::nullopt;
    }
    thread::MutexLock lock(&state_->mu);
    return state_->result;
  }

  // A host or guest fiber parks here. A guest event thread outside a fiber
  // fails clearly on an unresolved wait. The public deadline remains
  // absl::Time; its remaining duration is measured by the steady clock.
  absl::StatusOr<T> Await(absl::Time deadline = absl::InfiniteFuture()) const {
    if (!state_) {
      return absl::FailedPreconditionError("Future is invalid");
    }
    if (auto ready = ResultIfReady()) {
      return *ready;
    }
    if (!thread::CanParkAwait()) {
      return absl::FailedPreconditionError(
          "Await requires a supported fiber context");
    } else {
      const bool infinite = deadline == absl::InfiniteFuture();
      absl::Duration remaining =
          infinite ? absl::InfiniteDuration() : deadline - absl::Now();
      thread::MutexLock lock(&state_->mu);
      while (!state_->result) {
        if (!infinite && remaining <= absl::ZeroDuration()) {
          return absl::DeadlineExceededError("Future deadline expired");
        }
        if (infinite) {
          state_->ready.Wait(&state_->mu);
          continue;
        }
        const auto start = std::chrono::steady_clock::now();
        const bool timed_out =
            state_->ready.WaitWithTimeout(&state_->mu, remaining);
        const auto elapsed =
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - start);
        remaining -= absl::Nanoseconds(elapsed.count());
        if (timed_out && !state_->result) {
          return absl::DeadlineExceededError("Future deadline expired");
        }
      }
      return *state_->result;
    }
  }

  void OnReady(std::function<void(const absl::StatusOr<T>&)> callback) const {
    if (!callback) {
      return;
    }
    auto state = state_;
    if (!state) {
      const absl::StatusOr<T> invalid =
          absl::FailedPreconditionError("Future is not valid");
      callback(invalid);
      return;
    }
    const absl::StatusOr<T>* absl_nullable ready = nullptr;
    {
      thread::MutexLock lock(&state->mu);
      if (!state->result) {
        state->callbacks.push_back(std::move(callback));
        return;
      }
      ready = &*state->result;
    }
    callback(*ready);
  }

  // Guest-only opt-in placement. Include event_executor.h at the call site.
  // Then() and OnReady() keep their inline A11 behavior.
#if defined(__SYMBIAN32__)
  template <typename Fn>
  auto ThenOnWorker(EventExecutor* absl_nonnull executor, Fn transform) const
      -> Future<typename std::invoke_result_t<
          Fn, const absl::StatusOr<T>&>::value_type>;
#endif

 private:
  explicit Future(std::shared_ptr<internal::FutureState<T>> state)
      : state_(std::move(state)) {}

  std::shared_ptr<internal::FutureState<T>> state_;
  friend class Promise<T>;
};

template <typename T>
Future<T> ReadyFuture(T value) {
  Promise<T> promise;
  Future<T> future = promise.future();
  promise.SetValue(std::move(value));
  return future;
}

template <typename T>
Future<T> FailedFuture(absl::Status error) {
  Promise<T> promise;
  Future<T> future = promise.future();
  promise.SetError(error);
  return future;
}

using Task = Future<Unit>;

inline Task ReadyTask() {
  return ReadyFuture(Unit{});
}

inline Task FailedTask(absl::Status error) {
  return FailedFuture<Unit>(error);
}

template <typename T, typename Fn>
auto Then(const Future<T>& future, Fn transform) -> Future<
    typename std::invoke_result_t<Fn, const absl::StatusOr<T>&>::value_type> {
  using U =
      typename std::invoke_result_t<Fn, const absl::StatusOr<T>&>::value_type;
  if (auto ready = future.ResultIfReady()) {
    Promise<U> promise;
    Future<U> continued = promise.future();
    promise.SetResult(transform(*ready));
    return continued;
  }
  auto promise = std::make_shared<Promise<U>>();
  Future<U> continued = promise->future();
  promise->SetCancellationCallback([future] { future.Cancel(); });
  future.OnReady(
      [promise = std::move(promise), transform = std::move(transform)](
          const absl::StatusOr<T>& result) mutable {
        promise->SetResult(transform(result));
      });
  return continued;
}

}  // namespace symbian::concurrency

#endif  // SYMBIAN_CONCURRENCY_FUTURE_H_
