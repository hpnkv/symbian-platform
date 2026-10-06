// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.
// Thread-relative RTimer ownership for a single event-thread request pump.

#ifndef SYMBIAN_CONCURRENCY_NATIVE_TIMER_H_
#define SYMBIAN_CONCURRENCY_NATIVE_TIMER_H_

#if __has_include(<symbian/runtime.h>)

#include <absl/base/nullability.h>
#include <symbian/runtime.h>
#else
#include "abi.h"
#endif

namespace symbian::concurrency {

class NativeTimer {
 public:
  NativeTimer() = default;
  NativeTimer(const NativeTimer&) = delete;
  NativeTimer& operator=(const NativeTimer&) = delete;

  ~NativeTimer() { Close(); }

  // Create, arm, inspect and close on the same OS thread. A timer may be
  // rearmed only after its previous status has completed.
  int Open() {
    if (state_ != nullptr) {
      return -11;  // KErrAlreadyExists.
    }
    return SymbianRuntimeTimerCreate(&state_);
  }

  int Start(int microseconds) {
    return SymbianRuntimeTimerStart(state_, microseconds);
  }

  void Cancel() { SymbianRuntimeTimerCancel(state_); }

  int Result() const { return SymbianRuntimeTimerResult(state_); }

  bool IsReady() const { return SymbianRuntimeTimerIsReady(state_); }

  void Close() {
    if (state_ == nullptr) {
      return;
    }
    SymbianRuntimeTimerClose(state_);
    state_ = nullptr;
  }

 private:
  SymbianRuntimeTimerState* absl_nullable state_ = nullptr;
};

}  // namespace symbian::concurrency

#endif  // SYMBIAN_CONCURRENCY_NATIVE_TIMER_H_
