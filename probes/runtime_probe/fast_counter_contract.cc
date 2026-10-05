#include <chrono>
#include <cstdint>

#include "abi.h"

extern "C" int SymbianRuntimeFastCounterProbe() {
  const int frequency = SymbianRuntimeFastCounterFrequency();
  if (frequency != 32768) {
    return -237;
  }
  const auto start_time = std::chrono::steady_clock::now();
  const std::uint32_t start_count = SymbianRuntimeFastCounter();
  std::uint32_t end_count = start_count;
  auto end_time = start_time;
  while (end_time - start_time < std::chrono::seconds(1)) {
    end_count = SymbianRuntimeFastCounter();
    end_time = std::chrono::steady_clock::now();
  }
  const std::uint32_t count_delta = end_count - start_count;
  const std::int64_t elapsed_nanos =
      std::chrono::duration_cast<std::chrono::nanoseconds>(end_time -
                                                           start_time)
          .count();
  const std::int64_t expected = elapsed_nanos * frequency / 1000000000;
  if (count_delta + frequency / 100 < expected ||
      count_delta > expected + frequency / 100) {
    return -238;
  }
#ifdef SYMBIAN_RUNTIME_CHANGED_FAST_COUNTER
  return -239;
#else
  return 0;
#endif
}
