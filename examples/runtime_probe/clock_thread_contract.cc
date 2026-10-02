#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>

#include "abi.h"

namespace {

std::int64_t NowNanos() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

}  // namespace

extern "C" int SymbianRuntimeClockThreadProbe() {
  const int cells_before = SymbianRuntimeAllocationCells();
  std::atomic<int> start{0};
  std::atomic<int> turn{0};
  std::atomic<int> failures{0};
  std::atomic<std::int64_t> handoff{0};

  std::thread worker([&] {
    while (start.load(std::memory_order_acquire) == 0) {
      std::this_thread::yield();
    }
    std::int64_t previous = NowNanos();
    for (int i = 0; i < 2048; ++i) {
      const std::int64_t current = NowNanos();
      if (current < previous) {
        failures.fetch_add(1, std::memory_order_relaxed);
      }
      previous = current;
    }
    for (int round = 0; round < 128; ++round) {
      while (turn.load(std::memory_order_acquire) != 2 * round) {
        std::this_thread::yield();
      }
      const std::int64_t current = NowNanos();
      if (current < handoff.load(std::memory_order_acquire)) {
        failures.fetch_add(1, std::memory_order_relaxed);
      }
      handoff.store(current, std::memory_order_release);
      turn.store(2 * round + 1, std::memory_order_release);
    }
  });

  start.store(1, std::memory_order_release);
  std::int64_t previous = NowNanos();
  for (int i = 0; i < 2048; ++i) {
    const std::int64_t current = NowNanos();
    if (current < previous) {
      failures.fetch_add(1, std::memory_order_relaxed);
    }
    previous = current;
  }
  for (int round = 0; round < 128; ++round) {
    while (turn.load(std::memory_order_acquire) != 2 * round + 1) {
      std::this_thread::yield();
    }
    const std::int64_t current = NowNanos();
    if (current < handoff.load(std::memory_order_acquire)) {
      failures.fetch_add(1, std::memory_order_relaxed);
    }
    handoff.store(current, std::memory_order_release);
    turn.store(2 * round + 2, std::memory_order_release);
  }
  worker.join();
  if (failures.load(std::memory_order_acquire) != 0) {
    return -242;
  }
  if (SymbianRuntimeAllocationCells() != cells_before) {
    return -241;
  }
#ifdef SYMBIAN_RUNTIME_CHANGED_CLOCK_THREAD
  return -240;
#else
  return 0;
#endif
}
