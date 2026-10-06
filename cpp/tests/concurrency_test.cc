// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include <memory>
#include <thread>
#include <vector>

#include <absl/base/nullability.h>

#include "absl/status/status.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "gtest/gtest.h"
#include "symbian/concurrency/bounded_channel.h"
#include "symbian/concurrency/event_mailbox.h"
#include "symbian/concurrency/future.h"
#include "symbian/concurrency/task_group.h"
#include "thread/channel.h"
#include "thread/select.h"
#include "thread/selectables.h"

namespace {

TEST(ConcurrencyTest, A11ChannelKeepsSelectableReaderWriterInterface) {
  thread::Channel<std::unique_ptr<int>> channel(1);
  auto value = std::make_unique<int>(17);
  EXPECT_EQ(thread::Select({channel.writer()->OnWrite(std::move(value))}), 0);
  EXPECT_FALSE(value);
  EXPECT_EQ(channel.length(), 1);
  std::unique_ptr<int> received;
  bool read_ok = false;
  EXPECT_EQ(thread::Select({channel.reader()->OnRead(&received, &read_ok)}), 0);
  EXPECT_TRUE(read_ok);
  ASSERT_TRUE(received);
  EXPECT_EQ(*received, 17);
  EXPECT_TRUE(
      channel.writer()->WriteUnlessCancelled(std::make_unique<int>(19)));
  EXPECT_TRUE(channel.reader()->Read(&received));
  ASSERT_TRUE(received);
  EXPECT_EQ(*received, 19);
  channel.writer()->Close();
  EXPECT_FALSE(channel.reader()->Read(&received));
}

TEST(ConcurrencyTest, ChannelDrainsAndRetainsRejectedMoveOnlyValue) {
  symbian::concurrency::BoundedChannel<std::unique_ptr<int>> channel(1);
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
  symbian::concurrency::BoundedChannel<int> channel(1);
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
  symbian::concurrency::EventMailbox* absl_nullable mailbox_ptr = nullptr;
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

TEST(ConcurrencyTest, ReadyThenRunsInlineAndPropagatesStatus) {
  bool ran = false;
  auto ready = symbian::concurrency::ReadyFuture(41);
  auto continued = symbian::concurrency::Then(
      ready, [&](const absl::StatusOr<int>& value) -> absl::StatusOr<int> {
        ran = true;
        return *value + 1;
      });
  EXPECT_TRUE(ran);
  ASSERT_TRUE(continued.IsReady());
  ASSERT_TRUE(continued.ResultIfReady()->ok());
  EXPECT_EQ(**continued.ResultIfReady(), 42);

  auto failed = symbian::concurrency::FailedFuture<int>(
      absl::CancelledError("upstream cancelled"));
  auto propagated = symbian::concurrency::Then(
      failed, [](const absl::StatusOr<int>& value) -> absl::StatusOr<int> {
        return value;
      });
  ASSERT_TRUE(propagated.IsReady());
  EXPECT_EQ(propagated.ResultIfReady()->status().code(),
            absl::StatusCode::kCancelled);
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
