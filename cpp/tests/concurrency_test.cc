// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include <memory>
#include <thread>
#include <vector>

#include "absl/status/status.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "gtest/gtest.h"
#include "symbian/concurrency/event_mailbox.h"
#include "symbian/concurrency/future.h"
#include "symbian/concurrency/task_group.h"
#include "thread/channel.h"

namespace {

TEST(ConcurrencyTest, ChannelDrainsAndRetainsRejectedMoveOnlyValue) {
  thread::Channel<std::unique_ptr<int>> channel(1);
  auto first = std::make_unique<int>(7);
  auto rejected = std::make_unique<int>(11);
  ASSERT_TRUE(channel.TryWrite(std::move(first)).ok());
  EXPECT_FALSE(first);
  EXPECT_EQ(channel.TryWrite(std::move(rejected)).code(),
            absl::StatusCode::kResourceExhausted);
  ASSERT_TRUE(rejected);
  EXPECT_EQ(*rejected, 11);
  channel.Close();
  std::unique_ptr<int> value;
  ASSERT_TRUE(channel.Read(&value));
  ASSERT_TRUE(value);
  EXPECT_EQ(*value, 7);
  EXPECT_FALSE(channel.Read(&value));
  EXPECT_EQ(channel.Write(std::move(rejected)).code(),
            absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(*rejected, 11);
}

TEST(ConcurrencyTest, ChannelCloseWakesBlockedWriter) {
  thread::Channel<int> channel(1);
  ASSERT_TRUE(channel.Write(1).ok());
  absl::Status write_result;
  std::thread writer([&] { write_result = channel.Write(2); });
  channel.Close();
  writer.join();
  EXPECT_EQ(write_result.code(), absl::StatusCode::kFailedPrecondition);
  int value = 0;
  EXPECT_TRUE(channel.Read(&value));
  EXPECT_EQ(value, 1);
  EXPECT_FALSE(channel.Read(&value));
}

TEST(ConcurrencyTest, FutureTaskGroupAndMailboxSharePortableLayer) {
  using symbian::concurrency::Promise;
  using symbian::concurrency::TaskGroup;
  using symbian::concurrency::Unit;

  Promise<Unit> first;
  Promise<Unit> second;
  TaskGroup group;
  ASSERT_TRUE(group.Add(first.future()));
  ASSERT_TRUE(group.Add(second.future()));
  auto joined = group.Finish();
  EXPECT_FALSE(joined.IsReady());
  EXPECT_FALSE(group.Finish().IsReady());
  EXPECT_TRUE(first.SetValue(Unit{}));
  EXPECT_FALSE(joined.IsReady());
  EXPECT_TRUE(second.SetValue(Unit{}));
  ASSERT_TRUE(joined.IsReady());
  ASSERT_TRUE(joined.ResultIfReady().has_value());
  EXPECT_TRUE(joined.ResultIfReady()->ok());
  EXPECT_TRUE(group.Finish().ResultIfReady()->ok());

  std::vector<int> calls;
  symbian::concurrency::EventMailbox* mailbox_ptr = nullptr;
  symbian::concurrency::EventMailbox mailbox([] {}, 2);
  mailbox_ptr = &mailbox;
  ASSERT_TRUE(mailbox
                  .Enqueue([&] {
                    calls.push_back(1);
                    EXPECT_TRUE(
                        mailbox_ptr->Enqueue([&] { calls.push_back(3); }).ok());
                  })
                  .ok());
  ASSERT_TRUE(mailbox.Enqueue([&] { calls.push_back(2); }).ok());
  EXPECT_EQ(mailbox.DispatchReady(2), 2);
  EXPECT_EQ(calls, (std::vector<int>{1, 2}));
  EXPECT_EQ(mailbox.DispatchReady(2), 1);
  EXPECT_EQ(calls, (std::vector<int>{1, 2, 3}));
}

TEST(ConcurrencyTest, CondVarUsesA11TimeoutConvention) {
  thread::Mutex mu;
  thread::CondVar condition;
  mu.Lock();
  EXPECT_TRUE(condition.WaitWithDeadline(&mu, absl::Now() - absl::Seconds(1)));
  EXPECT_TRUE(condition.WaitWithTimeout(&mu, absl::Milliseconds(2)));
  mu.Unlock();
}

#if SYMBIAN_CONCURRENCY_HOST_BOOST
TEST(ConcurrencyTest, HostAwaitParksAndHonorsDeadline) {
  symbian::concurrency::Promise<int> promise;
  auto future = promise.future();
  std::thread producer([&] {
    thread::SleepFor(absl::Milliseconds(1));
    promise.SetValue(42);
  });
  absl::StatusOr<int> value = future.Await(absl::Now() + absl::Seconds(1));
  producer.join();
  ASSERT_TRUE(value.ok()) << value.status();
  EXPECT_EQ(*value, 42);

  symbian::concurrency::Promise<int> late;
  EXPECT_EQ(late.future().Await(absl::Now() - absl::Seconds(1)).status().code(),
            absl::StatusCode::kDeadlineExceeded);
}
#endif

}  // namespace
