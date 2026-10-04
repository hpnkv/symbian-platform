// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_CONCURRENCY_WORKER_EXECUTOR_H_
#define SYMBIAN_CONCURRENCY_WORKER_EXECUTOR_H_

#include <cstddef>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "symbian/concurrency/future.h"

namespace symbian::concurrency {

/**
 * @brief Bounded work destination on one guest OS thread.
 *
 * Use for operations too long for the event thread. Post executes without a
 * fiber; PostFiber schedules a job on the worker's A11-derived fiber scheduler.
 * The admission limit counts queued and active jobs. Close does not block the
 * event thread; Finish reports drainage.
 */
class WorkerExecutor {
 private:
  struct State;

 public:
  using Work = absl::AnyInvocable<void() &&>;

  class DispatchHandle {
   public:
    /** @brief Enqueue stackless work or return a full/closed status. */
    absl::Status Post(Work work) const;

   private:
    friend class WorkerExecutor;

    explicit DispatchHandle(std::weak_ptr<State> state)
        : state_(std::move(state)) {}

    std::weak_ptr<State> state_;
  };

  explicit WorkerExecutor(std::size_t max_outstanding = 128);
  WorkerExecutor(const WorkerExecutor&) = delete;
  WorkerExecutor& operator=(const WorkerExecutor&) = delete;
  ~WorkerExecutor();

  DispatchHandle handle() const { return DispatchHandle(state_); }

  /** @brief Enqueue stackless work on the single worker thread. */
  absl::Status Post(Work work) { return handle().Post(std::move(work)); }

  /**
   * @brief Run one job on a bounded guest fiber hosted by this worker.
   *
   * Use a larger stack for native libraries with deep call chains. The stack
   * is allocated only for the live job and may be 4 KiB to 1 MiB, aligned to
   * a machine word. Admission still uses max_outstanding.
   */
  Task PostFiber(Work work, std::size_t stack_bytes = 16 * 1024);
  /** @brief Stop accepting work while queued jobs continue to drain. */
  void Close();
  /** @brief Close and return a task that completes after all jobs drain. */
  Task Finish();

 private:
  std::shared_ptr<State> state_;
};

namespace internal {
absl::StatusOr<WorkerExecutor*> WorkerFor(EventExecutor& executor);
}  // namespace internal

// The transform is posted as a stackless callback. The completing thread
// only enqueues a cheap Future handle; the worker copies the result there.
// Queue rejection completes the returned Future with an error. Then()
// retains its A11 inline semantics.
template <typename T, typename Fn>
auto ThenOn(const Future<T>& future, WorkerExecutor& worker, Fn transform)
    -> Future<typename std::invoke_result_t<
        Fn, const absl::StatusOr<T>&>::value_type> {
  using U =
      typename std::invoke_result_t<Fn, const absl::StatusOr<T>&>::value_type;
  auto promise = std::make_shared<Promise<U>>();
  Future<U> continued = promise->future();
  promise->SetCancellationCallback([future] { future.Cancel(); });
  auto handle = worker.handle();
  future.OnReady([promise = std::move(promise),
                  transform = std::move(transform), handle,
                  future](const absl::StatusOr<T>&) mutable {
    absl::Status posted = handle.Post(
        [promise, transform = std::move(transform), future]() mutable {
          auto result = future.ResultIfReady();
          if (!result) {
            promise->SetError(absl::InternalError(
                "Ready Future lost its result before worker dispatch"));
            return;
          }
          promise->SetResult(transform(*result));
        });
    if (!posted.ok()) {
      promise->SetError(std::move(posted));
    }
  });
  return continued;
}

template <typename T>
template <typename Fn>
auto Future<T>::ThenOnWorker(EventExecutor& executor, Fn transform) const
    -> Future<typename std::invoke_result_t<
        Fn, const absl::StatusOr<T>&>::value_type> {
  using U =
      typename std::invoke_result_t<Fn, const absl::StatusOr<T>&>::value_type;
  auto worker = internal::WorkerFor(executor);
  if (!worker.ok()) {
    return FailedFuture<U>(worker.status());
  }
  return ThenOn(*this, **worker, std::move(transform));
}

}  // namespace symbian::concurrency

#endif  // SYMBIAN_CONCURRENCY_WORKER_EXECUTOR_H_
