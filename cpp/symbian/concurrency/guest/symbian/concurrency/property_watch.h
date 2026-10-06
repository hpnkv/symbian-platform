// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_CONCURRENCY_PROPERTY_WATCH_H_
#define SYMBIAN_CONCURRENCY_PROPERTY_WATCH_H_

#include <atomic>
#include <functional>
#include <memory>
#include <utility>

#include <absl/base/nullability.h>

#include "symbian/concurrency/future.h"
#include "symbian/native_status.h"

#if __has_include(<symbian/runtime.h>)
#include <symbian/runtime.h>
#else
#include "abi.h"
#endif

namespace symbian::concurrency {

// Owns one integer Publish & Subscribe property and one outstanding change
// request. Only the event thread calls Open, Next, Set, DispatchReady and Close.
// Worker threads may request Future cancellation through the supplied wakeup.
class PropertyWatch {
 public:
  explicit PropertyWatch(std::function<void()> wake) : wake_(std::move(wake)) {}

  PropertyWatch(const PropertyWatch&) = delete;
  PropertyWatch& operator=(const PropertyWatch&) = delete;

  ~PropertyWatch() { Close(); }

  absl::Status Open(int category, unsigned int key) {
    if (closed_ || native_ != nullptr) {
      return absl::FailedPreconditionError("Property owner already opened");
    }
    return symbian::StatusFromNativeError(
        SymbianRuntimePropertyCreate(category, key, &native_),
        "RProperty define and attach");
  }

  Future<int> Next() {
    if (closed_ || native_ == nullptr) {
      return FailedFuture<int>(
          absl::FailedPreconditionError("Property owner is closed"));
    }
    if (current_ != nullptr) {
      return FailedFuture<int>(
          absl::FailedPreconditionError("Property request already pending"));
    }
    auto entry = std::make_shared<Entry>();
    Future<int> future = entry->promise.future();
    entry->promise.SetCancellationCallback(
        [entry = std::weak_ptr<Entry>(entry), wake = wake_] {
          if (auto owned = entry.lock()) {
            owned->cancel_requested.store(true, std::memory_order_release);
            wake();
          }
        });
    current_ = entry;  // Own state before Subscribe can complete inline.
    const int submitted = SymbianRuntimePropertySubscribe(native_);
    if (submitted != symbian::native_error::kNone) {
      current_.reset();
      entry->promise.SetError(
          symbian::StatusFromNativeError(submitted, "RProperty subscribe"));
    }
    return future;
  }

  absl::Status Set(int value) {
    if (closed_ || native_ == nullptr) {
      return absl::FailedPreconditionError("Property owner is closed");
    }
    return symbian::StatusFromNativeError(
        SymbianRuntimePropertySet(native_, value), "RProperty set");
  }

  bool DispatchReady() {
    auto entry = current_;
    if (closed_ || entry == nullptr) {
      return false;
    }
    if (entry->cancel_requested.load(std::memory_order_acquire)) {
      SymbianRuntimePropertyCancel(native_);
    }
    if (!SymbianRuntimePropertyIsReady(native_)) {
      return false;
    }
    int value = 0;
    const int result = SymbianRuntimePropertyResult(native_, &value);
    current_.reset();  // Remove before an inline OnReady callback.
    if (result == symbian::native_error::kNone) {
      entry->promise.SetValue(value);
    } else {
      entry->promise.SetError(
          symbian::StatusFromNativeError(result, "RProperty change"));
    }
    return true;
  }

  void Close() {
    if (closed_) {
      return;
    }
    closed_ = true;
    if (native_ != nullptr) {
      SymbianRuntimePropertyClose(native_);  // Cancels and drains first.
      native_ = nullptr;
    }
    auto entry = std::move(current_);
    if (entry != nullptr) {
      entry->promise.SetError(absl::CancelledError("Property owner closed"));
    }
  }

 private:
  struct Entry {
    Promise<int> promise;
    std::atomic<bool> cancel_requested{false};
  };

  std::function<void()> wake_;
  SymbianRuntimePropertyState* absl_nullable native_ = nullptr;
  std::shared_ptr<Entry> current_;
  bool closed_ = false;
};

}  // namespace symbian::concurrency

#endif  // SYMBIAN_CONCURRENCY_PROPERTY_WATCH_H_
