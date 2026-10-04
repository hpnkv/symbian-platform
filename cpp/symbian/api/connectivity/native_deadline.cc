// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "native_deadline.h"

#include <cstdint>

#include <e32base.h>
#include <es_sock.h>
#include <limits.h>
#include <time.h>

namespace symbian::api::connectivity {

int WaitForSocketRequest(RSocket& socket, TRequestStatus& request,
                         std::int64_t deadline, void (RSocket::*cancel)()) {
  if (deadline == INT64_MAX) {
    User::WaitForRequest(request);
    return request.Int();
  }
  RTimer timer;
  const TInt opened = timer.CreateLocal();
  if (opened != KErrNone) {
    (socket.*cancel)();
    User::WaitForRequest(request);
    return opened;
  }
  timespec now{};
  if (clock_gettime(CLOCK_REALTIME, &now) != 0) {
    (socket.*cancel)();
    User::WaitForRequest(request);
    timer.Close();
    return KErrGeneral;
  }
  const std::int64_t now_microseconds =
      static_cast<std::int64_t>(now.tv_sec) * 1000000 + now.tv_nsec / 1000;
  std::int64_t remaining =
      deadline > now_microseconds ? deadline - now_microseconds : 0;
  constexpr TInt kMaximumNativeInterval = 30 * 60 * 1000000;
  while (request.Int() == KRequestPending) {
    if (remaining == 0) {
      (socket.*cancel)();
      User::WaitForRequest(request);
      timer.Close();
      return KErrTimedOut;
    }
    const TInt interval = remaining < kMaximumNativeInterval
                              ? static_cast<TInt>(remaining)
                              : kMaximumNativeInterval;
    TRequestStatus alarm;
    timer.After(alarm, TTimeIntervalMicroSeconds32(interval));
    User::WaitForRequest(request, alarm);
    timer.Cancel();
    if (alarm.Int() == KRequestPending) {
      User::WaitForRequest(alarm);
    }
    if (request.Int() == KRequestPending) {
      remaining -= interval;
    }
  }
  const TInt result = request.Int();
  timer.Close();
  return result;
}

}  // namespace symbian::api::connectivity
