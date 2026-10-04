// Copyright 2026 The Action Engine Authors and the Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.
// Adapted from A11 cpp/thread/thread/select.cc at
// fcccb8cb6e1e67d7ba0822ac14cece9ee4c7091b.

#include "thread/select.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <memory>

namespace thread {
namespace {

// Rotation avoids a permanent first-case preference without pulling the host
// random subsystem into the guest's bounded Abseil closure.
std::atomic<std::size_t> next_first_case{0};

internal::CaseStateArray MakeCaseStates(std::size_t count) {
  internal::CaseStateArray states(count);
  const std::size_t first =
      next_first_case.fetch_add(1, std::memory_order_relaxed) % count;
  for (std::size_t i = 0; i < count; ++i) {
    states[i].index = static_cast<int>((first + i) % count);
  }
  return states;
}

}  // namespace

bool internal::Selector::WaitForPickFor(absl::Duration remaining) {
  const bool infinite = remaining == absl::InfiniteDuration();
  const absl::Duration accepted = remaining;
  const auto start = std::chrono::steady_clock::now();
  while (picked_case_index == kNonePicked) {
    if (infinite) {
      cv.Wait(&mu);
      continue;
    }
    if (remaining <= absl::ZeroDuration()) {
      return false;
    }
    cv.WaitWithTimeout(&mu, remaining);
    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - start);
    remaining = accepted - absl::Nanoseconds(elapsed.count());
  }
  return true;
}

int SelectUntil(absl::Time deadline, const CaseArray& cases) {
  if (cases.empty()) {
    std::abort();
  }
  const bool infinite = deadline == absl::InfiniteFuture();
  const absl::Duration remaining =
      infinite ? absl::InfiniteDuration() : deadline - absl::Now();
  // The pinned int-returning API has no error slot for a finite duration that
  // cannot be represented after wall-to-monotonic conversion.
  if (!infinite && remaining == absl::InfiniteDuration()) {
    std::abort();
  }
  const bool enqueue = infinite || remaining > absl::ZeroDuration();
  auto selector = std::make_shared<internal::Selector>();
  auto states = MakeCaseStates(cases.size());
  std::size_t registered = 0;
  for (auto& state : states) {
    state.case_ptr = &cases[static_cast<std::size_t>(state.index)];
    state.selector = selector;
    ++registered;
    if (state.Handle(enqueue)) {
      break;
    }
  }

  int selected = internal::Selector::kNonePicked;
  bool expired = false;
  {
    MutexLock lock(&selector->mu);
    if (selector->picked_case_index == internal::Selector::kNonePicked) {
      expired =
          !selector->WaitForPickFor(enqueue ? remaining : absl::ZeroDuration());
      if (expired) {
        selector->picked_case_index = static_cast<int>(cases.size());
      }
    }
    selected = selector->picked_case_index;
  }
  if (enqueue) {
    for (std::size_t i = 0; i < registered; ++i) {
      if (states[i].index != selected) {
        states[i].Unregister();
      }
    }
  }
  return expired ? -1 : selected;
}

}  // namespace thread
