// Copyright 2026 The Action Engine Authors and the Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.
// Adapted from A11 cpp/thread/thread/selectables.h at
// fcccb8cb6e1e67d7ba0822ac14cece9ee4c7091b.

#ifndef THREAD_FIBER_SELECTABLES_H_
#define THREAD_FIBER_SELECTABLES_H_

#include <atomic>

#include "thread/cases.h"

namespace thread {

class PermanentEvent final : public internal::Selectable {
 public:
  PermanentEvent() = default;
  PermanentEvent(const PermanentEvent&) = delete;
  PermanentEvent& operator=(const PermanentEvent&) = delete;
  ~PermanentEvent() override;

  void Notify();
  bool HasBeenNotified() const;

  Case OnEvent() const { return {const_cast<PermanentEvent*>(this)}; }

  bool Handle(internal::CaseInSelectClause* case_state, bool enqueue) override;
  void Unregister(internal::CaseInSelectClause* case_state) override;

 private:
  mutable Mutex mu_;
  std::atomic<bool> notified_{false};
  internal::CaseInSelectClause* cases_to_be_selected_ = nullptr;
};

Case NonSelectableCase();
Case AlwaysSelectableCase();

}  // namespace thread

#endif  // THREAD_FIBER_SELECTABLES_H_
