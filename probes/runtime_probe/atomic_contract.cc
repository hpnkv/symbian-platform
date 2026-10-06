#include <atomic>

#include <absl/base/nullability.h>

extern "C" unsigned char SymbianSyncLockTestAndSet1(
    volatile void* absl_nonnull pointer,
    unsigned char value) asm("__sync_lock_test_and_set_1");
extern "C" unsigned int SymbianSyncFetchAndAnd4(
    volatile void* absl_nonnull pointer,
    unsigned int value) asm("__sync_fetch_and_and_4");
extern "C" unsigned int SymbianSyncFetchAndOr4(
    volatile void* absl_nonnull pointer,
    unsigned int value) asm("__sync_fetch_and_or_4");

extern "C" int SymbianRuntimeAtomicProbe() {
  std::atomic<int> value{2};
  if (value.fetch_add(3, std::memory_order_acq_rel) != 2 ||
      value.load(std::memory_order_acquire) != 5) {
    return -133;
  }
  int expected = 4;
  if (value.compare_exchange_strong(expected, 9, std::memory_order_acq_rel) ||
      expected != 5) {
    return -134;
  }
  if (!value.compare_exchange_strong(expected, 9, std::memory_order_acq_rel) ||
      value.exchange(11, std::memory_order_acq_rel) != 9 ||
      value.load(std::memory_order_acquire) != 11 ||
      value.fetch_sub(4, std::memory_order_acq_rel) != 11 ||
      value.load(std::memory_order_acquire) != 7) {
    return -135;
  }
  unsigned char byte = 7;
  if (SymbianSyncLockTestAndSet1(&byte, 11) != 7 || byte != 11 ||
      SymbianSyncLockTestAndSet1(&byte, 3) != 11 || byte != 3) {
    return -137;
  }
  unsigned int bits = 0b1110;
  if (SymbianSyncFetchAndAnd4(&bits, 0b1011) != 0b1110 || bits != 0b1010 ||
      SymbianSyncFetchAndOr4(&bits, 0b0101) != 0b1010 || bits != 0b1111) {
    return -141;
  }
  std::atomic<unsigned int> atomic_bits{0b1110};
  if (atomic_bits.fetch_and(0b1011, std::memory_order_acq_rel) != 0b1110 ||
      atomic_bits.fetch_or(0b0101, std::memory_order_acq_rel) != 0b1010 ||
      atomic_bits.load(std::memory_order_acquire) != 0b1111) {
    return -142;
  }
  std::atomic<unsigned char> atomic_byte{4};
  if (!atomic_byte.is_lock_free() ||
      atomic_byte.load(std::memory_order_acquire) != 4) {
    return -138;
  }
  atomic_byte.store(6, std::memory_order_release);
  unsigned char expected_byte = 5;
  if (atomic_byte.compare_exchange_strong(expected_byte, 9,
                                          std::memory_order_acq_rel) ||
      expected_byte != 6 ||
      !atomic_byte.compare_exchange_strong(expected_byte, 9,
                                           std::memory_order_acq_rel) ||
      atomic_byte.exchange(11, std::memory_order_acq_rel) != 9 ||
      atomic_byte.load(std::memory_order_acquire) != 11) {
    return -139;
  }
#ifdef SYMBIAN_RUNTIME_CHANGED_ATOMIC
  return value.load(std::memory_order_acquire) == 6 ? 0 : -136;
#else
  return 0;
#endif
}
