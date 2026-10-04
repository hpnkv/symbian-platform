// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "symbian/concurrency/worker_executor.h"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#include "symbian/concurrency/event_executor.h"
#include "thread/fiber.h"

extern "C" void SymbianRuntimeThreadCacheEnter();
extern "C" void SymbianRuntimeThreadCacheLeave();

namespace symbian::concurrency {

absl::StatusOr<WorkerExecutor*> internal::WorkerFor(EventExecutor& executor) {
  return executor.workers();
}

struct WorkerExecutor::State : std::enable_shared_from_this<State> {
  struct Job {
    Work work;
    std::shared_ptr<Promise<Unit>> done;
  };

  explicit State(std::size_t limit) : max_outstanding(limit) {}

  absl::Status Enqueue(Job job) {
    if (!job.work) {
      return absl::InvalidArgumentError("Worker work is empty");
    }
    {
      std::lock_guard lock(mu);
      if (closing) {
        return absl::FailedPreconditionError("Worker executor is closed");
      }
      if (outstanding >= max_outstanding) {
        return absl::ResourceExhaustedError("Worker executor is full");
      }
      queue.push_back(std::move(job));
      ++outstanding;
      ++wake_sequence;
    }
    cv.notify_one();
    return absl::OkStatus();
  }

  void Wake() noexcept {
    {
      std::lock_guard lock(mu);
      ++wake_sequence;
    }
    cv.notify_one();
  }

  struct Policy final : thread::SchedulerPolicy {
    explicit Policy(State& owner) : owner(owner) {}

    std::size_t PickNext(std::span<thread::Fiber* const>) override { return 0; }

    void NotifyReady() noexcept override { owner.Wake(); }

    State& owner;
  };

  void Run() {
    SymbianRuntimeThreadCacheEnter();

    struct CacheScope {
      ~CacheScope() { SymbianRuntimeThreadCacheLeave(); }
    } cache_scope;

    Policy policy(*this);
    thread::Scheduler scheduler(&policy);
    std::vector<std::unique_ptr<thread::Fiber>> fibers;
    while (true) {
      Job job;
      bool have_job = false;
      {
        std::lock_guard lock(mu);
        if (!queue.empty()) {
          job = std::move(queue.front());
          queue.pop_front();
          have_job = true;
        } else if (closing && fibers.empty()) {
          break;
        }
      }
      if (have_job) {
        if (job.done) {
          auto done = std::move(job.done);
          fibers.push_back(std::make_unique<thread::Fiber>(
              scheduler, [work = std::move(job.work), done]() mutable {
                std::move(work)();
                done->SetValue(Unit{});
              }));
        } else {
          std::move(job.work)();
          std::lock_guard lock(mu);
          --outstanding;
        }
      }
      if (scheduler.HasReady()) {
        if (!scheduler.RunReady(16).ok()) {
          std::abort();
        }
      }
      const auto reaped = std::erase_if(
          fibers, [](const auto& fiber) { return fiber->Finished(); });
      if (reaped != 0) {
        std::lock_guard lock(mu);
        outstanding -= reaped;
      }
      if (have_job || scheduler.HasReady()) {
        continue;
      }
      std::unique_lock lock(mu);
      if (!queue.empty() || (closing && fibers.empty())) {
        continue;
      }
      const auto seen = wake_sequence;
      lock.unlock();
      if (scheduler.HasReady()) {
        continue;
      }
      const auto deadline = scheduler.NextDeadline();
      lock.lock();
      if (deadline == std::chrono::steady_clock::time_point::max()) {
        cv.wait(lock, [this, seen] { return wake_sequence != seen; });
      } else {
        cv.wait_until(lock, deadline,
                      [this, seen] { return wake_sequence != seen; });
      }
    }
    finished_promise.SetValue(Unit{});
  }

  const std::size_t max_outstanding;
  std::mutex mu;
  std::condition_variable cv;
  std::deque<Job> queue;
  std::size_t outstanding = 0;
  std::size_t wake_sequence = 0;
  bool closing = false;
  Promise<Unit> finished_promise;
  Task finished = finished_promise.future();
};

WorkerExecutor::WorkerExecutor(std::size_t max_outstanding)
    : state_(std::make_shared<State>(max_outstanding)) {
  std::thread([state = state_] { state->Run(); }).detach();
}

WorkerExecutor::~WorkerExecutor() {
  Close();
}

absl::Status WorkerExecutor::DispatchHandle::Post(Work work) const {
  auto state = state_.lock();
  if (!state) {
    return absl::FailedPreconditionError("Worker executor is gone");
  }
  return state->Enqueue(State::Job{std::move(work), {}});
}

Task WorkerExecutor::PostFiber(Work work) {
  auto promise = std::make_shared<Promise<Unit>>();
  Task task = promise->future();
  absl::Status status =
      state_->Enqueue(State::Job{std::move(work), std::move(promise)});
  if (!status.ok()) {
    return FailedTask(std::move(status));
  }
  return task;
}

void WorkerExecutor::Close() {
  if (!state_) {
    return;
  }
  {
    std::lock_guard lock(state_->mu);
    state_->closing = true;
    ++state_->wake_sequence;
  }
  state_->cv.notify_one();
}

Task WorkerExecutor::Finish() {
  Close();
  return state_->finished;
}

}  // namespace symbian::concurrency
