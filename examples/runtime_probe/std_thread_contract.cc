#include <atomic>
#include <memory>
#include <thread>
#include <utility>

#include "abi.h"

extern "C" int SymbianRuntimeStdThreadProbe() {
  const int before = SymbianRuntimeAllocationCells();
  std::atomic<int> counter{0};
  std::atomic<int> completed{0};
  auto shared = std::make_shared<int>(2026);
  std::weak_ptr<int> weak = shared;
  auto owned = std::make_unique<int>(14);
  std::thread worker(
      [shared, owned = std::move(owned), &counter, &completed]() mutable {
        for (int i = 0; i < 2000; ++i) {
          counter.fetch_add(1, std::memory_order_acq_rel);
        }
        completed.store(*shared + *owned, std::memory_order_release);
      });
  for (int i = 0; i < 2000; ++i) {
    counter.fetch_add(1, std::memory_order_acq_rel);
  }
  worker.join();
  if (counter.load(std::memory_order_acquire) != 4000 || *shared != 2026 ||
      shared.use_count() != 1 ||
      completed.load(std::memory_order_acquire) != 2040 || owned) {
    return -151;
  }
  shared.reset();
  if (!weak.expired()) {
    return -152;
  }
  weak.reset();
  if (SymbianRuntimeAllocationCells() != before) {
    return -153;
  }
#ifdef SYMBIAN_RUNTIME_CHANGED_STD_THREAD
  return -154;
#else
  return 0;
#endif
}
