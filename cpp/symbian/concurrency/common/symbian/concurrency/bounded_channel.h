// Copyright 2026 The Action Engine Authors and the Symbian SDK Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SDK-owned fallible bounded queue, separate from A11
// cpp/thread/thread/channel.h at fcccb8cb6e1e67d7ba0822ac14cece9ee4c7091b.
// This queue serves nonblocking SDK mailboxes. It does not change the
// signatures or closed-write contract of thread::Channel.

#ifndef SYMBIAN_CONCURRENCY_BOUNDED_CHANNEL_H_
#define SYMBIAN_CONCURRENCY_BOUNDED_CHANNEL_H_

#include <cstddef>
#include <deque>
#include <type_traits>
#include <utility>

#include <absl/base/nullability.h>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "thread/boost_primitives.h"

namespace symbian::concurrency {

template <typename T>
class BoundedChannel;

template <typename T>
class BoundedReader {
 public:
  explicit BoundedReader(BoundedChannel<T>* absl_nonnull channel)
      : channel_(channel) {}

  bool Read(T* absl_nonnull out) { return channel_->Read(out); }

  absl::StatusOr<bool> TryRead(T* absl_nonnull out) {
    return channel_->TryRead(out);
  }

 private:
  BoundedChannel<T>* absl_nonnull channel_;
};

template <typename T>
class BoundedWriter {
 public:
  explicit BoundedWriter(BoundedChannel<T>* absl_nonnull channel)
      : channel_(channel) {}

  absl::Status Write(T&& item) { return channel_->Write(std::move(item)); }

  absl::Status TryWrite(T&& item) {
    return channel_->TryWrite(std::move(item));
  }

  absl::Status Write(const T& item) requires std::is_copy_constructible_v<T> {
    return channel_->Write(item);
  }

  absl::Status TryWrite(
      const T& item) requires std::is_copy_constructible_v<T> {
    return channel_->TryWrite(item);
  }

  void Close() { channel_->Close(); }

 private:
  BoundedChannel<T>* absl_nonnull channel_;
};

// A bounded multi-producer, multi-consumer FIFO. Capacity must be positive.
// Blocking calls park according to the selected thread primitive backend:
// Boost fibers on hosts, guest ARM fibers on the event thread, or an OS thread
// outside a fiber. A failed write
// preserves an rvalue source. Closed writes return Status under the default
// no-exceptions profile.
template <typename T>
class BoundedChannel {
  static_assert(std::is_move_assignable_v<T>);

 public:
  explicit BoundedChannel(std::size_t capacity)
      : capacity_(capacity), reader_(this), writer_(this) {}

  BoundedChannel(const BoundedChannel&) = delete;
  BoundedChannel& operator=(const BoundedChannel&) = delete;

  BoundedReader<T>* absl_nonnull reader() { return &reader_; }

  BoundedWriter<T>* absl_nonnull writer() { return &writer_; }

  absl::Status TryWrite(T&& item) { return TryWriteMoved(&item); }

  absl::Status TryWrite(
      const T& item) requires std::is_copy_constructible_v<T> {
    {
      thread::MutexLock lock(&mu_);
      if (absl::Status ready = CheckWritable(); !ready.ok()) {
        return ready;
      }
      queue_.push_back(item);
    }
    readers_.Signal();
    return absl::OkStatus();
  }

  absl::Status Write(T&& item) { return WriteMoved(&item); }

  absl::Status Write(const T& item) requires std::is_copy_constructible_v<T> {
    {
      thread::MutexLock lock(&mu_);
      if (capacity_ == 0) {
        return absl::InvalidArgumentError(
            "BoundedChannel capacity must be positive");
      }
      while (!closed_ && queue_.size() >= capacity_) {
        writers_.Wait(&mu_);
      }
      if (closed_) {
        return absl::FailedPreconditionError("BoundedChannel is closed");
      }
      queue_.push_back(item);
    }
    readers_.Signal();
    return absl::OkStatus();
  }

  // True means one item was read; false means closed and drained. An open,
  // empty channel returns Unavailable so pollers can distinguish the states.
  absl::StatusOr<bool> TryRead(T* absl_nonnull out) {
    {
      thread::MutexLock lock(&mu_);
      if (queue_.empty()) {
        if (closed_) {
          return false;
        }
        return absl::UnavailableError("BoundedChannel is empty");
      }
      *out = std::move(queue_.front());
      queue_.pop_front();
    }
    writers_.Signal();
    return true;
  }

  bool Read(T* absl_nonnull out) {
    {
      thread::MutexLock lock(&mu_);
      while (!closed_ && queue_.empty()) {
        readers_.Wait(&mu_);
      }
      if (queue_.empty()) {
        return false;
      }
      *out = std::move(queue_.front());
      queue_.pop_front();
    }
    writers_.Signal();
    return true;
  }

  // Close rejects writes, wakes blocked readers/writers, and permits queued
  // items to drain. Repeating Close is harmless for owner shutdown.
  void Close() {
    {
      thread::MutexLock lock(&mu_);
      closed_ = true;
    }
    readers_.SignalAll();
    writers_.SignalAll();
  }

  // Discard closes and releases queued values outside the queue lock.
  void Discard() {
    std::deque<T> retired;
    {
      thread::MutexLock lock(&mu_);
      closed_ = true;
      retired.swap(queue_);
    }
    readers_.SignalAll();
    writers_.SignalAll();
  }

  std::size_t Size() const {
    thread::MutexLock lock(&mu_);
    return queue_.size();
  }

 private:
  absl::Status CheckWritable() const {
    if (closed_) {
      return absl::FailedPreconditionError("BoundedChannel is closed");
    }
    if (capacity_ == 0) {
      return absl::InvalidArgumentError(
          "BoundedChannel capacity must be positive");
    }
    if (queue_.size() >= capacity_) {
      return absl::ResourceExhaustedError("BoundedChannel is full");
    }
    return absl::OkStatus();
  }

  absl::Status TryWriteMoved(T* absl_nonnull item) {
    {
      thread::MutexLock lock(&mu_);
      if (absl::Status ready = CheckWritable(); !ready.ok()) {
        return ready;
      }
      queue_.push_back(std::move(*item));
    }
    readers_.Signal();
    return absl::OkStatus();
  }

  absl::Status WriteMoved(T* absl_nonnull item) {
    {
      thread::MutexLock lock(&mu_);
      if (capacity_ == 0) {
        return absl::InvalidArgumentError(
            "BoundedChannel capacity must be positive");
      }
      while (!closed_ && queue_.size() >= capacity_) {
        writers_.Wait(&mu_);
      }
      if (closed_) {
        return absl::FailedPreconditionError("BoundedChannel is closed");
      }
      queue_.push_back(std::move(*item));
    }
    readers_.Signal();
    return absl::OkStatus();
  }

  const std::size_t capacity_;
  mutable thread::Mutex mu_;
  thread::CondVar readers_;
  thread::CondVar writers_;
  std::deque<T> queue_;
  bool closed_ = false;
  BoundedReader<T> reader_;
  BoundedWriter<T> writer_;
};

}  // namespace symbian::concurrency

#endif  // SYMBIAN_CONCURRENCY_BOUNDED_CHANNEL_H_
