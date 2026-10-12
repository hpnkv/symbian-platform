// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "symbian/api/system/failure_report_tasks.h"

#include <atomic>
#include <thread>

#include "absl/status/status.h"
#include "absl/time/time.h"
#include "gtest/gtest.h"
#include "symbian/concurrency/future.h"
#include "thread/boost_primitives.h"
#include "thread/fiber.h"

namespace {

using symbian::api::system::internal::WaitForFailureReportTasks;
using symbian::concurrency::Promise;
using symbian::concurrency::ReadyTask;
using symbian::concurrency::Unit;

TEST(FailureReportTasksTest, UiFailureDoesNotCancelOrAbandonPendingWrite) {
  Promise<Unit> view;
  Promise<Unit> write;
  int cancelled = 0;
  write.SetCancellationCallback([&] { ++cancelled; });
  absl::Status status;
  thread::Fiber waiter([&] {
    status = WaitForFailureReportTasks(view.future(), write.future());
  });
  thread::Fiber producer([&] {
    view.SetError(absl::UnavailableError("Window Server unavailable"));
    thread::SleepFor(absl::Milliseconds(1));
    EXPECT_FALSE(waiter.Finished());
    EXPECT_EQ(cancelled, 0);
    write.SetValue(Unit{});
  });
  EXPECT_TRUE(waiter.Join().ok());
  EXPECT_TRUE(producer.Join().ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kUnavailable);
  EXPECT_EQ(cancelled, 0);
}

TEST(FailureReportTasksTest, WindowDiesBeforeSlowWriteAndJoinCompletes) {
  Promise<Unit> view;
  Promise<Unit> write;
  bool window_alive = false;
  bool write_finished = false;
  absl::Status status;
  thread::Fiber waiter([&] {
    status = WaitForFailureReportTasks(view.future(), write.future());
    EXPECT_TRUE(write_finished);
  });
  thread::Fiber ui([&] {
    struct Window {
      bool* absl_nonnull alive;
      ~Window() { *alive = false; }
    };
    {
      Window window{&window_alive};
      window_alive = true;
      thread::SleepFor(absl::Milliseconds(1));
    }
    view.SetValue(Unit{});
  });
  thread::Fiber writer([&] {
    EXPECT_TRUE(view.future().Await().ok());
    EXPECT_FALSE(window_alive);
    EXPECT_FALSE(waiter.Finished());
    write_finished = true;
    write.SetValue(Unit{});
  });
  EXPECT_TRUE(waiter.Join().ok());
  EXPECT_TRUE(ui.Join().ok());
  EXPECT_TRUE(writer.Join().ok());
  EXPECT_TRUE(status.ok());
}

TEST(FailureReportTasksTest, WriteFailureDoesNotCloseOrCancelView) {
  Promise<Unit> view;
  Promise<Unit> write;
  int cancelled = 0;
  view.SetCancellationCallback([&] { ++cancelled; });
  absl::Status status;
  thread::Fiber waiter([&] {
    status = WaitForFailureReportTasks(view.future(), write.future());
  });
  thread::Fiber producer([&] {
    write.SetError(absl::ResourceExhaustedError("Disk full"));
    thread::SleepFor(absl::Milliseconds(1));
    EXPECT_FALSE(waiter.Finished());
    EXPECT_EQ(cancelled, 0);
    view.SetValue(Unit{});
  });
  EXPECT_TRUE(waiter.Join().ok());
  EXPECT_TRUE(producer.Join().ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kResourceExhausted);
}

TEST(FailureReportTasksTest, CompletedWriteStillWaitsForUserToCloseView) {
  Promise<Unit> view;
  absl::Status status;
  thread::Fiber waiter(
      [&] { status = WaitForFailureReportTasks(view.future(), ReadyTask()); });
  thread::Fiber ui([&] {
    thread::SleepFor(absl::Milliseconds(1));
    EXPECT_FALSE(waiter.Finished());
    view.SetValue(Unit{});
  });
  EXPECT_TRUE(waiter.Join().ok());
  EXPECT_TRUE(ui.Join().ok());
  EXPECT_TRUE(status.ok());
}

TEST(FailureReportTasksTest, NativeCallerWaitsForBothResultsAcrossThreads) {
  Promise<Unit> view;
  Promise<Unit> write;
  std::atomic<bool> returned{false};
  absl::Status status;
  std::thread caller([&] {
    status = WaitForFailureReportTasks(view.future(), write.future());
    returned.store(true);
  });
  write.SetError(absl::DataLossError("Flush failed"));
  EXPECT_FALSE(returned.load());
  view.SetError(absl::UnavailableError("Presentation failed"));
  caller.join();
  EXPECT_TRUE(returned.load());
  EXPECT_EQ(status.code(), absl::StatusCode::kUnavailable);
}

}  // namespace
