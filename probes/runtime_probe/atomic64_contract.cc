#include <atomic>
#include <cstdint>

extern "C" void SymbianRuntimeThreadYield();
extern "C" int SymbianRuntimeRunThread(void* state, int (*worker)(void*),
                                       int (*parent)(void*));
extern "C" std::uint64_t SymbianProbeSyncAdd8(
    volatile void* pointer, std::uint64_t value) asm("__sync_fetch_and_add_8");
extern "C" std::uint64_t SymbianProbeSyncSub8(
    volatile void* pointer, std::uint64_t value) asm("__sync_fetch_and_sub_8");
extern "C" std::uint64_t SymbianProbeSyncCas8(
    volatile void* pointer, std::uint64_t expected,
    std::uint64_t desired) asm("__sync_val_compare_and_swap_8");
extern "C" std::uint64_t SymbianProbeSyncSwap8(
    volatile void* pointer,
    std::uint64_t desired) asm("__sync_lock_test_and_set_8");

namespace {

struct Shared {
  std::atomic<int> ready{0};
  std::atomic<std::uint64_t> value{0};
};

int Increment(void* context) {
  auto& state = *static_cast<Shared*>(context);
  state.ready.fetch_add(1, std::memory_order_acq_rel);
  for (int attempt = 0; attempt < 3000; ++attempt) {
    if (state.ready.load(std::memory_order_acquire) == 2) {
      for (int index = 0; index < 2000; ++index) {
        state.value.fetch_add(0x100000001ULL, std::memory_order_acq_rel);
      }
      return 0;
    }
    SymbianRuntimeThreadYield();
  }
  return -207;
}

}  // namespace

extern "C" int SymbianRuntimeAtomic64Probe() {
  std::atomic<std::uint64_t> seed{0x100000002ULL};
#ifdef SYMBIAN_RUNTIME_EXPECT_NATIVE_ATOMIC64
  if (!seed.is_lock_free()) {
    return -216;
  }
#else
  if (seed.is_lock_free()) {
    return -216;
  }
#endif
  if (seed.load(std::memory_order_acquire) != 0x100000002ULL) {
    return -213;
  }
  std::atomic<std::uint64_t> stored{0};
  stored.store(0x200000003ULL, std::memory_order_seq_cst);
  if (stored.load(std::memory_order_seq_cst) != 0x200000003ULL) {
    return -218;
  }
  std::uint64_t seed_expected = 0x100000002ULL;
  if (!seed.compare_exchange_strong(seed_expected, 0x200000003ULL,
                                    std::memory_order_acq_rel)) {
    return -215;
  }
  if (seed.load(std::memory_order_acquire) != 0x200000003ULL) {
    return -214;
  }
  std::uint64_t reflected = 0x100000001ULL;
  std::atomic_ref<std::uint64_t> reference(reflected);
  if (std::atomic_ref<std::uint64_t>::is_always_lock_free ||
      reference.is_lock_free() != seed.is_lock_free() ||
      reference.fetch_add(0x100000001ULL) != 0x100000001ULL ||
      reflected != 0x200000002ULL) {
    return -217;
  }
  Shared state;
  if (int result = SymbianRuntimeRunThread(&state, Increment, Increment);
      result != 0) {
    return result;
  }
  const std::uint64_t total = 0xFA000000FA0ULL;
  if (state.value.load(std::memory_order_acquire) != total) {
    return -208;
  }
  std::uint64_t expected = total - 1;
  if (state.value.compare_exchange_strong(expected, 42,
                                          std::memory_order_acq_rel) ||
      expected != total) {
    return -209;
  }
  if (!state.value.compare_exchange_strong(expected, 0x100000002ULL,
                                           std::memory_order_acq_rel) ||
      state.value.exchange(0x200000003ULL, std::memory_order_acq_rel) !=
          0x100000002ULL) {
    return -210;
  }
  alignas(8) volatile std::uint64_t raw = 0x100000004ULL;
  if (SymbianProbeSyncAdd8(&raw, 0x100000001ULL) != 0x100000004ULL ||
      SymbianProbeSyncSub8(&raw, 2) != 0x200000005ULL ||
      SymbianProbeSyncCas8(&raw, 0x100000003ULL, 7) != 0x200000003ULL ||
      SymbianProbeSyncCas8(&raw, 0x200000003ULL, 9) != 0x200000003ULL ||
      SymbianProbeSyncSwap8(&raw, 0x300000004ULL) != 9) {
    return -211;
  }
#ifdef SYMBIAN_RUNTIME_CHANGED_ATOMIC64
  return state.value.load(std::memory_order_acquire) == 0x200000002ULL ? 0
                                                                       : -212;
#else
  return state.value.load(std::memory_order_acquire) == 0x200000003ULL &&
                 raw == 0x300000004ULL
             ? 0
             : -212;
#endif
}
