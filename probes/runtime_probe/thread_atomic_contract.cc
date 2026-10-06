#include <atomic>

#include <absl/base/nullability.h>

extern "C" void SymbianRuntimeThreadYield();
extern "C" int SymbianRuntimeRunThread(
    void* absl_nullable state, int (*absl_nonnull worker)(void* absl_nonnull),
    int (*absl_nonnull parent)(void* absl_nonnull));

namespace {

struct Shared {
  std::atomic<int> ready{0};
  std::atomic<int> counter{0};
};

int Increment(void* absl_nonnull context) {
  auto& state = *static_cast<Shared*>(context);
  state.ready.fetch_add(1, std::memory_order_acq_rel);
  for (int attempt = 0; attempt < 3000; ++attempt) {
    if (state.ready.load(std::memory_order_acquire) == 2) {
      for (int i = 0; i < 2000; ++i) {
        state.counter.fetch_add(1, std::memory_order_acq_rel);
      }
      return 0;
    }
    SymbianRuntimeThreadYield();
  }
  return -139;
}

}  // namespace

extern "C" int SymbianRuntimeThreadAtomicProbe() {
  Shared state;
  const int result = SymbianRuntimeRunThread(&state, Increment, Increment);
  if (result != 0) {
    return result;
  }
#ifdef SYMBIAN_RUNTIME_CHANGED_THREAD_ATOMIC
  return state.counter.load(std::memory_order_acquire) == 3999 ? 0 : -140;
#else
  return state.counter.load(std::memory_order_acquire) == 4000 ? 0 : -140;
#endif
}
