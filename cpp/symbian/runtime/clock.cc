#include <atomic>
#include <cstdint>
#include <limits>

#include "abi.h"

namespace {

std::atomic<std::uint64_t> last_ticks{
    std::numeric_limits<std::uint64_t>::max()};
// Positive selects the nanokernel counter; negative selects the ordinary
// system tick if the firmware cannot report the nanokernel period.
std::atomic<int> tick_mode_period{0};

}  // namespace

extern "C" std::int64_t SymbianRuntimeSteadyClockNanoseconds() {
  int mode = tick_mode_period.load(std::memory_order_acquire);
  if (mode == 0) {
    int proposed = SymbianRuntimeNanoTickPeriodMicros();
    if (proposed <= 0) {
      const int fallback = SymbianRuntimeTickPeriodMicros();
      if (fallback <= 0) {
        SymbianRuntimeExit(SymbianRuntimeExitReason::kRuntimeContractFailure);
      }
      proposed = -fallback;
    }
    int empty = 0;
    if (tick_mode_period.compare_exchange_strong(empty, proposed,
                                                 std::memory_order_acq_rel,
                                                 std::memory_order_acquire)) {
      mode = proposed;
    } else {
      mode = empty;
    }
  }

  const int period = mode > 0 ? mode : -mode;
  const std::uint32_t raw =
      mode > 0 ? SymbianRuntimeNanoTickCount() : SymbianRuntimeTickCount();
  std::uint64_t previous = last_ticks.load(std::memory_order_acquire);
  while (true) {
    std::uint64_t next;
    if (previous == std::numeric_limits<std::uint64_t>::max()) {
      next = raw;
    } else {
      const std::int32_t difference =
          static_cast<std::int32_t>(raw - static_cast<std::uint32_t>(previous));
      if (difference <= 0) {
        next = previous;
      } else {
        next = previous + static_cast<std::uint32_t>(difference);
      }
    }
    if (last_ticks.compare_exchange_weak(previous, next,
                                         std::memory_order_acq_rel,
                                         std::memory_order_acquire)) {
      return static_cast<std::int64_t>(next) * period * 1000;
    }
  }
}
