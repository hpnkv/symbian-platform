// Copyright 2026 The Action Engine Authors and the Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.
// Adapted from A11 cpp/thread/thread/select.h at
// fcccb8cb6e1e67d7ba0822ac14cece9ee4c7091b.

#ifndef THREAD_FIBER_SELECT_H_
#define THREAD_FIBER_SELECT_H_

#include "thread/cases.h"

namespace thread {

// Returns a selected case index or -1 on expiry. A ready case wins over an
// already-expired deadline. The deadline is accepted as wall time and then
// measured by the verified monotonic clock.
int SelectUntil(absl::Time deadline, const CaseArray& cases);

inline int Select(const CaseArray& cases) {
  return SelectUntil(absl::InfiniteFuture(), cases);
}

}  // namespace thread

#endif  // THREAD_FIBER_SELECT_H_
