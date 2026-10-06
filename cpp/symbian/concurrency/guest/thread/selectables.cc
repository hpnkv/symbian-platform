// Copyright 2026 The Action Engine Authors and the Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.
// Adapted from A11 cpp/thread/thread/selectables.cc at
// fcccb8cb6e1e67d7ba0822ac14cece9ee4c7091b.

#include "thread/selectables.h"

#include <cstdlib>

#include <absl/base/nullability.h>

#include "absl/container/inlined_vector.h"

namespace thread {

PermanentEvent::~PermanentEvent() {
  MutexLock lock(&mu_);
  if (cases_to_be_selected_ != nullptr) {
    std::abort();
  }
}

bool PermanentEvent::Handle(
    internal::CaseInSelectClause* absl_nonnull case_state, bool enqueue) {
  MutexLock lock(&mu_);
  if (notified_.load(std::memory_order_relaxed)) {
    MutexLock selector_lock(&case_state->selector->mu);
    return case_state->TryPick();
  }
  if (enqueue) {
    internal::PushBack(&cases_to_be_selected_, case_state);
  }
  return false;
}

void PermanentEvent::Unregister(
    internal::CaseInSelectClause* absl_nonnull case_state) {
  MutexLock lock(&mu_);
  if (!notified_.load(std::memory_order_relaxed)) {
    internal::UnlinkFromList(&cases_to_be_selected_, case_state);
  }
}

void PermanentEvent::Notify() {
  absl::InlinedVector<std::shared_ptr<internal::Selector>, 4> wake;
  {
    MutexLock lock(&mu_);
    if (notified_.load(std::memory_order_relaxed)) {
      std::abort();
    }
    notified_.store(true, std::memory_order_release);
    while (cases_to_be_selected_ != nullptr) {
      auto* absl_nonnull case_state = cases_to_be_selected_;
      MutexLock selector_lock(&case_state->selector->mu);
      if (case_state->TryPick()) {
        wake.push_back(case_state->selector);
      }
      internal::UnlinkFromList(&cases_to_be_selected_, case_state);
    }
  }
  // Waking a fiber may call the scheduler policy. It must happen after the
  // event and selector locks have been released.
  for (const auto& selector : wake) {
    selector->cv.Signal();
  }
}

bool PermanentEvent::HasBeenNotified() const {
  return notified_.load(std::memory_order_acquire);
}

namespace {
class NonSelectable final : public internal::Selectable {
 public:
  bool Handle(internal::CaseInSelectClause* absl_nonnull, bool) override {
    return false;
  }

  void Unregister(internal::CaseInSelectClause* absl_nonnull) override {}
};

class AlwaysSelectable final : public internal::Selectable {
 public:
  bool Handle(internal::CaseInSelectClause* absl_nonnull case_state,
              bool) override {
    MutexLock lock(&case_state->selector->mu);
    return case_state->TryPick();
  }

  void Unregister(internal::CaseInSelectClause* absl_nonnull) override {}
};

NonSelectable g_non_selectable;
AlwaysSelectable g_always_selectable;
}  // namespace

Case NonSelectableCase() {
  return {&g_non_selectable};
}

Case AlwaysSelectableCase() {
  return {&g_always_selectable};
}

}  // namespace thread
