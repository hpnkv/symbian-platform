// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_CONCURRENCY_EVENT_MAILBOX_H_
#define SYMBIAN_CONCURRENCY_EVENT_MAILBOX_H_

#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "thread/channel.h"

namespace symbian::concurrency {

// A bounded cross-thread dispatch mailbox for an existing OS event thread.
// It is not A11's Post/PostAt shared pool and never consumes the native
// request semaphore; the caller drives it beside its native request owners.
class EventMailbox {
 public:
  explicit EventMailbox(std::function<void()> wake,
                        std::size_t max_pending = 128)
      : wake_(std::move(wake)), queue_(max_pending) {}

  EventMailbox(const EventMailbox&) = delete;
  EventMailbox& operator=(const EventMailbox&) = delete;

  ~EventMailbox() { Close(); }

  absl::Status Enqueue(std::function<void()> callback) {
    absl::Status status = queue_.TryWrite(std::move(callback));
    if (!status.ok()) {
      return status;
    }
    wake_();
    return absl::OkStatus();
  }

  // Execute no more than budget callbacks, always outside the mailbox lock.
  // A callback that enqueues more work is processed on a later turn.
  std::size_t DispatchReady(std::size_t budget = 64) {
    if (budget == 0) {
      return 0;
    }
    std::vector<std::function<void()>> ready;
    const std::size_t initial = queue_.Size();
    const std::size_t count = initial < budget ? initial : budget;
    ready.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
      std::function<void()> callback;
      absl::StatusOr<bool> read = queue_.TryRead(&callback);
      if (!read.ok() || !read.value()) {
        break;
      }
      ready.push_back(std::move(callback));
    }
    for (auto& callback : ready) {
      callback();
    }
    if (Pending() != 0) {
      wake_();
    }
    return ready.size();
  }

  std::size_t Pending() const { return queue_.Size(); }

  void Close() { queue_.Discard(); }

 private:
  std::function<void()> wake_;
  thread::Channel<std::function<void()>> queue_;
};

}  // namespace symbian::concurrency

#endif  // SYMBIAN_CONCURRENCY_EVENT_MAILBOX_H_
