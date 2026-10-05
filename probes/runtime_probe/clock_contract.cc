#include <chrono>

#include "abi.h"

extern "C" int SymbianRuntimeClockProbe() {
  if (SymbianRuntimeNanoTickPeriodMicros() != 1000) {
    return -237;
  }
  using Clock = std::chrono::steady_clock;
  if (!Clock::is_steady) {
    return -232;
  }
  const auto start = Clock::now();
  auto previous = start;
  for (int iteration = 0; iteration < 65536; ++iteration) {
    const auto current = Clock::now();
    if (current < previous) {
      return -233;
    }
    previous = current;
    if (iteration >= 4096 && previous > start) {
      break;
    }
  }
  if (previous <= start) {
    return -234;
  }
  const auto wall = std::chrono::system_clock::now();
  if (std::chrono::system_clock::to_time_t(wall) < 1700000000) {
    return -235;
  }
#ifdef SYMBIAN_RUNTIME_CHANGED_CLOCK
  return -236;
#else
  return 0;
#endif
}
