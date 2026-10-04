// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.
// The consumer uses only thread::Fiber; Boost stays inside the SDK backend.

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include "absl/status/status.h"
#include "absl/time/time.h"
#include "gtest/gtest.h"
#include "symbian/concurrency/future.h"
#include "thread/boost_primitives.h"
#include "thread/executor.h"
#include "thread/fiber.h"
#include "thread/select.h"
#include "thread/selectables.h"

namespace {

TEST(ConcurrencyFiberTest, MutexConditionAndSleepParkSdkFibers) {
  thread::Mutex mu;
  thread::CondVar condition;
  bool ready = false;
  int observed = 0;
  thread::Fiber waiter([&] {
    thread::MutexLock lock(&mu);
    while (!ready) {
      condition.Wait(&mu);
    }
    observed = 19;
  });
  thread::Fiber notifier([&] {
    thread::SleepFor(absl::Milliseconds(1));
    thread::MutexLock lock(&mu);
    ready = true;
    condition.Signal();
  });
  EXPECT_TRUE(waiter.Join().ok());
  EXPECT_TRUE(notifier.Join().ok());
  EXPECT_EQ(observed, 19);
  EXPECT_TRUE(waiter.Finished());
  EXPECT_EQ(waiter.Join().code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(thread::Fiber::Current(), nullptr);
}

TEST(ConcurrencyFiberTest, CooperativeCancelRunsCppCleanupAndPinsOwner) {
  std::atomic<int> destructed{0};

  struct Cleanup {
    std::atomic<int>* count;

    ~Cleanup() { count->fetch_add(1, std::memory_order_relaxed); }
  };

  std::thread::id owner = std::this_thread::get_id();
  bool affinity_ok = false;
  thread::Fiber worker([&] {
    Cleanup cleanup{&destructed};
    affinity_ok = std::this_thread::get_id() == owner;
    while (!thread::Fiber::Current()->Cancelled()) {
      thread::SleepFor(absl::Milliseconds(1));
    }
  });
  absl::Status wrong_thread;
  std::thread outsider([&] { wrong_thread = worker.Join(); });
  outsider.join();
  EXPECT_EQ(wrong_thread.code(), absl::StatusCode::kFailedPrecondition);
  worker.Cancel();
  EXPECT_TRUE(worker.Join().ok());
  EXPECT_TRUE(worker.Finished());
  EXPECT_TRUE(affinity_ok);
  EXPECT_EQ(destructed.load(std::memory_order_relaxed), 1);
}

TEST(ConcurrencyFiberTest, CancellationIsSelectable) {
  int selected = -1;
  thread::Fiber waiter(
      [&] { selected = thread::Select({thread::OnCancel()}); });
  waiter.Cancel();
  EXPECT_TRUE(waiter.Join().ok());
  EXPECT_EQ(selected, 0);
}

TEST(ConcurrencyFiberTest, FutureAwaitParksAnSdkFiber) {
  symbian::concurrency::Promise<int> promise;
  auto future = promise.future();
  int observed = 0;
  thread::Fiber waiter([&] {
    auto value = future.Await(absl::Now() + absl::Seconds(1));
    observed = value.ok() ? *value : -1;
  });
  thread::Fiber producer([&] {
    thread::SleepFor(absl::Milliseconds(1));
    promise.SetValue(23);
  });
  EXPECT_TRUE(waiter.Join().ok());
  EXPECT_TRUE(producer.Join().ok());
  EXPECT_EQ(observed, 23);
}

TEST(ConcurrencyFiberTest, SchedulerPolicyAndHostLockParkGuard) {
  class LastReadyPolicy final : public thread::SchedulerPolicy {
   public:
    size_t PickNext(size_t ready_count) noexcept override {
      picks.fetch_add(1, std::memory_order_relaxed);
      return ready_count - 1;
    }

    std::atomic<int> picks{0};
  };

  auto policy = std::make_shared<LastReadyPolicy>();
  std::atomic<int> releases{0};
  std::atomic<int> acquires{0};
  thread::SetSchedulerParkGuard(thread::SchedulerParkGuard{
      .release = [&]() -> void* {
        releases.fetch_add(1, std::memory_order_relaxed);
        return nullptr;
      },
      .acquire =
          [&](void*) { acquires.fetch_add(1, std::memory_order_relaxed); },
  });
  std::thread worker([&] {
    EXPECT_TRUE(thread::SetCurrentSchedulerPolicy(policy).ok());
    std::vector<int> order;
    thread::Fiber first([&] { order.push_back(1); });
    thread::Fiber second([&] { order.push_back(2); });
    thread::Fiber third([&] {
      thread::SleepFor(absl::Milliseconds(5));
      order.push_back(3);
    });
    EXPECT_TRUE(first.Join().ok());
    EXPECT_TRUE(second.Join().ok());
    EXPECT_TRUE(third.Join().ok());
    EXPECT_EQ(order.size(), 3);
    EXPECT_EQ(order[0], 2);
    EXPECT_EQ(order[1], 1);
    EXPECT_EQ(order[2], 3);
    EXPECT_EQ(thread::SetCurrentSchedulerPolicy(policy).code(),
              absl::StatusCode::kFailedPrecondition);
  });
  worker.join();
  thread::SetSchedulerParkGuard({});
  EXPECT_GT(policy->picks.load(std::memory_order_relaxed), 0);
  EXPECT_GT(releases.load(std::memory_order_relaxed), 0);
  EXPECT_EQ(releases.load(std::memory_order_relaxed),
            acquires.load(std::memory_order_relaxed));
}

TEST(ConcurrencyFiberTest, SharedPoolRunsStacklessPostsAndAbsoluteTimers) {
  std::atomic<int> completed{0};
  std::thread::id worker_id;
  const std::thread::id caller = std::this_thread::get_id();
  for (int index = 0; index < 256; ++index) {
    thread::Post([&, payload = std::make_unique<int>(index)] {
      if (*payload == 0) {
        worker_id = std::this_thread::get_id();
      }
      completed.fetch_add(1, std::memory_order_release);
    });
  }
  const auto started = std::chrono::steady_clock::now();
  std::atomic<bool> timer_fired{false};
  thread::PostAt(absl::Now() + absl::Milliseconds(20),
                 [&] { timer_fired.store(true, std::memory_order_release); });
  const auto limit = started + std::chrono::seconds(3);
  while ((completed.load(std::memory_order_acquire) != 256 ||
          !timer_fired.load(std::memory_order_acquire)) &&
         std::chrono::steady_clock::now() < limit) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  EXPECT_EQ(completed.load(std::memory_order_acquire), 256);
  EXPECT_TRUE(timer_fired.load(std::memory_order_acquire));
  EXPECT_NE(worker_id, caller);
  EXPECT_GE(std::chrono::steady_clock::now() - started,
            std::chrono::milliseconds(20));

  static std::atomic<int> never{0};
  std::atomic<int> past{0};
  thread::PostAt(absl::InfiniteFuture(),
                 [] { never.fetch_add(1, std::memory_order_relaxed); });
  thread::PostAt(absl::InfinitePast(),
                 [&] { past.fetch_add(1, std::memory_order_release); });
  const auto immediate_limit =
      std::chrono::steady_clock::now() + std::chrono::seconds(1);
  while (past.load(std::memory_order_acquire) == 0 &&
         std::chrono::steady_clock::now() < immediate_limit) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  EXPECT_EQ(past.load(std::memory_order_acquire), 1);
  EXPECT_EQ(never.load(std::memory_order_relaxed), 0);
}

TEST(ConcurrencyFiberTest, PermanentEventSelectParksAndIsLevelTriggered) {
  thread::PermanentEvent event;
  EXPECT_EQ(thread::SelectUntil(absl::InfinitePast(), {event.OnEvent()}), -1);
  int selected = -2;
  thread::Fiber waiter([&] {
    selected =
        thread::SelectUntil(absl::Now() + absl::Seconds(1), {event.OnEvent()});
  });
  thread::Fiber notifier([&] {
    thread::SleepFor(absl::Milliseconds(2));
    event.Notify();
  });
  EXPECT_TRUE(waiter.Join().ok());
  EXPECT_TRUE(notifier.Join().ok());
  EXPECT_EQ(selected, 0);
  EXPECT_TRUE(event.HasBeenNotified());
  EXPECT_EQ(thread::Select({event.OnEvent()}), 0);
  EXPECT_EQ(thread::Select({thread::AlwaysSelectableCase()}), 0);
  EXPECT_EQ(thread::SelectUntil(absl::Now() + absl::Milliseconds(2),
                                {thread::NonSelectableCase()}),
            -1);
}

}  // namespace
