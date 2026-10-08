// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include <cstdint>

#include <absl/base/nullability.h>
#include <e32std.h>
#include <e32svr.h>
#include <errno.h>
#include <time.h>
#include <u32hal.h>
#include <unistd.h>

#include "abi.h"

namespace {

constexpr std::int64_t kNanosecondsPerSecond = 1000000000;
constexpr std::int64_t kMicrosecondsPerSecond = 1000000;
constexpr std::int64_t kUnixEpochSymbianMicroseconds = 62168256000000000LL;

bool FillTimespec(std::int64_t value, std::int64_t units_per_second,
                  std::int64_t nanoseconds_per_unit,
                  timespec* absl_nonnull result) {
  std::int64_t seconds = value / units_per_second;
  std::int64_t remainder = value % units_per_second;
  if (remainder < 0) {
    --seconds;
    remainder += units_per_second;
  }
  const time_t stored_seconds = static_cast<time_t>(seconds);
  if (static_cast<std::int64_t>(stored_seconds) != seconds) {
    errno = EOVERFLOW;
    return false;
  }
  result->tv_sec = stored_seconds;
  result->tv_nsec = static_cast<long>(remainder * nanoseconds_per_unit);
  return true;
}

}  // namespace

extern "C" int clock_gettime(clockid_t clock, timespec* absl_nullable result) {
  if (result == nullptr) {
    errno = EFAULT;
    return -1;
  }
  if (clock == CLOCK_MONOTONIC) {
    return FillTimespec(SymbianRuntimeSteadyClockNanoseconds(),
                        kNanosecondsPerSecond, 1, result)
               ? 0
               : -1;
  }
  if (clock == CLOCK_REALTIME) {
    TTime current;
    current.UniversalTime();
    return FillTimespec(current.Int64() - kUnixEpochSymbianMicroseconds,
                        kMicrosecondsPerSecond, 1000, result)
               ? 0
               : -1;
  }
  errno = EINVAL;
  return -1;
}

extern "C" int nanosleep(const timespec* absl_nullable request,
                         timespec* absl_nullable remaining) {
  if (request == nullptr || request->tv_sec < 0 || request->tv_nsec < 0 ||
      request->tv_nsec >= kNanosecondsPerSecond) {
    errno = EINVAL;
    return -1;
  }
  std::int64_t seconds = request->tv_sec;
  while (seconds > 0) {
    const std::int64_t chunk = seconds > 2000 ? 2000 : seconds;
    if (SymbianRuntimeSleepMicroseconds(
            static_cast<std::uint64_t>(chunk * kMicrosecondsPerSecond)) != 0) {
      errno = EAGAIN;
      return -1;
    }
    seconds -= chunk;
  }
  if (request->tv_nsec > 0) {
    if (SymbianRuntimeSleepMicroseconds(
            static_cast<std::uint64_t>((request->tv_nsec + 999) / 1000)) != 0) {
      errno = EAGAIN;
      return -1;
    }
  }
  if (remaining != nullptr) {
    remaining->tv_sec = 0;
    remaining->tv_nsec = 0;
  }
  return 0;
}

extern "C" int sched_yield() {
  User::After(TTimeIntervalMicroSeconds32(0));
  return 0;
}

extern "C" long sysconf(int name) {
  if (name != _SC_NPROCESSORS_ONLN) {
    errno = EINVAL;
    return -1;
  }
  TInt processors = 0;
  const TInt result = UserSvr::HalFunction(
      EHalGroupKernel, EKernelHalNumLogicalCpus, &processors, nullptr);
  return result == KErrNone && processors > 0 ? processors : 1;
}
