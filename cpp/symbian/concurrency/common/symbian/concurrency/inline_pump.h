// Copyright 2026 The A11 Authors.
// Licensed under the Apache License, Version 2.0 (the "License");
// http://www.apache.org/licenses/LICENSE-2.0
//
// Guest no-exceptions adaptation of A11
// cpp/a11/concurrency/inline_pump.h at
// fcccb8cb6e1e67d7ba0822ac14cece9ee4c7091b.

#ifndef SYMBIAN_CONCURRENCY_INLINE_PUMP_H_
#define SYMBIAN_CONCURRENCY_INLINE_PUMP_H_

#include <cstddef>
#include <string_view>
#include <utility>

#include "thread/boost_primitives.h"

namespace symbian::concurrency {

struct InlinePumpState {
  size_t depth = 0;
  bool again = false;
};

template <typename Once>
void DriveInline(thread::Mutex* mu, InlinePumpState* state,
                 [[maybe_unused]] std::string_view name, Once&& once,
                 size_t max_depth = 4) {
  {
    thread::MutexLock lock(mu);
    if (state->depth >= max_depth) {
      state->again = true;
      return;
    }
    ++state->depth;
  }
  while (true) {
    once();
    thread::MutexLock lock(mu);
    if (!state->again) {
      --state->depth;
      return;
    }
    state->again = false;
  }
}

// The caller must hold the same mutex passed to DriveInline.
inline bool PumpIsDriving(const InlinePumpState& state) {
  return state.depth > 0;
}

}  // namespace symbian::concurrency

#endif  // SYMBIAN_CONCURRENCY_INLINE_PUMP_H_
