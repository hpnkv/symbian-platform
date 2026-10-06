// ARMv5T Clang lowers 32-bit C++ atomics to these libatomic ABI entry points.
// The ROM provides the operations and their inter-thread ordering. Keep the
// original SDK header separate from modern libc++ headers.

#include <absl/base/nullability.h>
#include <e32atomics.h>
#ifdef SYMBIAN_RUNTIME_MIMALLOC
#include <cstring>
#endif

static_assert(sizeof(TUint32) == 4);
static_assert(sizeof(TUint8) == 1);
static_assert(sizeof(TUint64) == 8);

// libc++ asks at run time. For the alternate profile, the caller explicitly
// selects a firmware with verified 64-bit EUSER exclusive operations.
extern "C" bool SymbianRuntimeAtomicIsLockFree(unsigned size) {
  if (size == 1) {
    return true;
  }
  if (size == 4) {
    return true;
  }
#ifdef SYMBIAN_RUNTIME_NATIVE_ATOMIC64
  if (size == 8) {
    return true;
  }
#endif
  return false;
}

extern "C" TUint64 SymbianRuntimeAtomic64Load(
    const volatile void* absl_nonnull pointer);
extern "C" void SymbianRuntimeAtomic64Store(volatile void* absl_nonnull pointer,
                                            TUint64 value);
extern "C" TUint64 SymbianRuntimeAtomic64Exchange(
    volatile void* absl_nonnull pointer, TUint64 value);
extern "C" bool SymbianRuntimeAtomic64CompareExchange(
    volatile void* absl_nonnull pointer, TUint64* absl_nonnull expected,
    TUint64 desired);
extern "C" TUint64 SymbianRuntimeAtomic64FetchAdd(
    volatile void* absl_nonnull pointer, TUint64 value);
#ifdef SYMBIAN_RUNTIME_MIMALLOC
extern "C" void SymbianRuntimeAtomicLock();
extern "C" void SymbianRuntimeAtomicUnlock();

extern "C" void SymbianAtomicLoadGeneric(
    size_t size, const volatile void* absl_nonnull pointer,
    void* absl_nonnull result, int) asm("__atomic_load");

extern "C" void SymbianAtomicLoadGeneric(
    size_t size, const volatile void* absl_nonnull pointer,
    void* absl_nonnull result, int) {
  SymbianRuntimeAtomicLock();
  std::memcpy(result, const_cast<const void*>(pointer), size);
  SymbianRuntimeAtomicUnlock();
}

extern "C" void SymbianAtomicStoreGeneric(size_t size,
                                          volatile void* absl_nonnull pointer,
                                          const void* absl_nonnull value,
                                          int) asm("__atomic_store");

extern "C" void SymbianAtomicStoreGeneric(size_t size,
                                          volatile void* absl_nonnull pointer,
                                          const void* absl_nonnull value, int) {
  SymbianRuntimeAtomicLock();
  std::memcpy(const_cast<void*>(pointer), value, size);
  SymbianRuntimeAtomicUnlock();
}
#endif

#ifdef SYMBIAN_RUNTIME_NATIVE_ATOMIC64
extern "C" TUint64 SymbianRuntimeAtomic64Load(
    const volatile void* absl_nonnull pointer) {
  return __e32_atomic_load_acq64(pointer);
}

extern "C" void SymbianRuntimeAtomic64Store(volatile void* absl_nonnull pointer,
                                            TUint64 value) {
  __e32_atomic_store_ord64(pointer, value);
}

extern "C" TUint64 SymbianRuntimeAtomic64Exchange(
    volatile void* absl_nonnull pointer, TUint64 value) {
  return __e32_atomic_swp_ord64(pointer, value);
}

extern "C" bool SymbianRuntimeAtomic64CompareExchange(
    volatile void* absl_nonnull pointer, TUint64* absl_nonnull expected,
    TUint64 desired) {
  return __e32_atomic_cas_ord64(pointer, expected, desired);
}

extern "C" TUint64 SymbianRuntimeAtomic64FetchAdd(
    volatile void* absl_nonnull pointer, TUint64 value) {
  return __e32_atomic_add_ord64(pointer, value);
}
#endif

extern "C" bool __atomic_compare_exchange_1(volatile void* absl_nonnull pointer,
                                            void* absl_nonnull expected,
                                            TUint8 desired, int, int) {
  return __e32_atomic_cas_ord8(pointer, static_cast<TUint8*>(expected),
                               desired);
}

extern "C" TUint8 __atomic_load_1(const volatile void* absl_nonnull pointer,
                                  int order) {
  if (order == __ATOMIC_SEQ_CST) {
    __e32_memory_barrier();
  }
  return __e32_atomic_load_acq8(pointer);
}

extern "C" void __atomic_store_1(volatile void* absl_nonnull pointer,
                                 TUint8 value, int) {
  __e32_atomic_store_ord8(pointer, value);
}

extern "C" TUint8 __atomic_exchange_1(volatile void* absl_nonnull pointer,
                                      TUint8 value, int) {
  return __e32_atomic_swp_ord8(pointer, value);
}

extern "C" TUint32 __atomic_fetch_add_4(volatile void* absl_nonnull pointer,
                                        TUint32 value, int) {
  return __e32_atomic_add_ord32(pointer, value);
}

extern "C" TUint32 __atomic_fetch_sub_4(volatile void* absl_nonnull pointer,
                                        TUint32 value, int) {
  return __e32_atomic_add_ord32(pointer, TUint32{0} - value);
}

extern "C" TUint32 __atomic_fetch_and_4(volatile void* absl_nonnull pointer,
                                        TUint32 value, int) {
  return __e32_atomic_and_ord32(pointer, value);
}

extern "C" TUint32 __atomic_fetch_or_4(volatile void* absl_nonnull pointer,
                                       TUint32 value, int) {
  return __e32_atomic_ior_ord32(pointer, value);
}

extern "C" TUint32 __atomic_load_4(const volatile void* absl_nonnull pointer,
                                   int order) {
  if (order == __ATOMIC_SEQ_CST) {
    __e32_memory_barrier();
  }
  return __e32_atomic_load_acq32(pointer);
}

extern "C" void __atomic_store_4(volatile void* absl_nonnull pointer,
                                 TUint32 value, int) {
  __e32_atomic_store_ord32(pointer, value);
}

extern "C" bool __atomic_compare_exchange_4(volatile void* absl_nonnull pointer,
                                            void* absl_nonnull expected,
                                            TUint32 desired, int, int) {
  return __e32_atomic_cas_ord32(pointer, static_cast<TUint32*>(expected),
                                desired);
}

extern "C" TUint32 __atomic_exchange_4(volatile void* absl_nonnull pointer,
                                       TUint32 value, int) {
  return __e32_atomic_swp_ord32(pointer, value);
}

// ARMv6 Thumb code generated by Clang uses the older __sync libcall ABI for
// these same operations. Return the pre-operation value as that ABI requires.
extern "C" TUint32 SymbianSyncFetchAndAdd4(
    volatile void* absl_nonnull pointer,
    TUint32 value) asm("__sync_fetch_and_add_4");

extern "C" TUint32 SymbianSyncFetchAndAdd4(volatile void* absl_nonnull pointer,
                                           TUint32 value) {
  return __e32_atomic_add_ord32(pointer, value);
}

extern "C" TUint32 SymbianSyncFetchAndAnd4(
    volatile void* absl_nonnull pointer,
    TUint32 value) asm("__sync_fetch_and_and_4");

extern "C" TUint32 SymbianSyncFetchAndAnd4(volatile void* absl_nonnull pointer,
                                           TUint32 value) {
  return __e32_atomic_and_ord32(pointer, value);
}

extern "C" TUint32 SymbianSyncFetchAndOr4(
    volatile void* absl_nonnull pointer,
    TUint32 value) asm("__sync_fetch_and_or_4");

extern "C" TUint32 SymbianSyncFetchAndOr4(volatile void* absl_nonnull pointer,
                                          TUint32 value) {
  return __e32_atomic_ior_ord32(pointer, value);
}

extern "C" TUint32 SymbianSyncCompareAndSwap4(
    volatile void* absl_nonnull pointer, TUint32 old_value,
    TUint32 new_value) asm("__sync_val_compare_and_swap_4");

extern "C" TUint32 SymbianSyncCompareAndSwap4(
    volatile void* absl_nonnull pointer, TUint32 old_value, TUint32 new_value) {
  __e32_atomic_cas_ord32(pointer, &old_value, new_value);
  return old_value;
}

extern "C" TUint32 SymbianSyncLockTestAndSet4(
    volatile void* absl_nonnull pointer,
    TUint32 value) asm("__sync_lock_test_and_set_4");

extern "C" TUint32 SymbianSyncLockTestAndSet4(
    volatile void* absl_nonnull pointer, TUint32 value) {
  return __e32_atomic_swp_ord32(pointer, value);
}

extern "C" TUint8 SymbianSyncCompareAndSwap1(
    volatile void* absl_nonnull pointer, TUint8 old_value,
    TUint8 new_value) asm("__sync_val_compare_and_swap_1");

extern "C" TUint8 SymbianSyncCompareAndSwap1(
    volatile void* absl_nonnull pointer, TUint8 old_value, TUint8 new_value) {
  __e32_atomic_cas_ord8(pointer, &old_value, new_value);
  return old_value;
}

// Compiler-generated __atomic/__sync calls have different spellings, but
// share return-old and expected-value contracts. The original-SDK bridge
// serializes them with one process-owned RFastLock.
extern "C" TUint64 SymbianAtomicLoad8(const volatile void* absl_nonnull pointer,
                                      int) asm("__atomic_load_8");

extern "C" TUint64 SymbianAtomicLoad8(const volatile void* absl_nonnull pointer,
                                      int order) {
  if (order == __ATOMIC_SEQ_CST) {
    __e32_memory_barrier();
  }
  return SymbianRuntimeAtomic64Load(pointer);
}

extern "C" void SymbianAtomicStore8(volatile void* absl_nonnull pointer,
                                    TUint64 value, int) asm("__atomic_store_8");

extern "C" void SymbianAtomicStore8(volatile void* absl_nonnull pointer,
                                    TUint64 value, int) {
  SymbianRuntimeAtomic64Store(pointer, value);
}

extern "C" TUint64 SymbianAtomicExchange8(volatile void* absl_nonnull pointer,
                                          TUint64 value,
                                          int) asm("__atomic_exchange_8");

extern "C" TUint64 SymbianAtomicExchange8(volatile void* absl_nonnull pointer,
                                          TUint64 value, int) {
  return SymbianRuntimeAtomic64Exchange(pointer, value);
}

extern "C" bool SymbianAtomicCompareExchange8(
    volatile void* absl_nonnull pointer, void* absl_nonnull expected,
    TUint64 desired, int, int) asm("__atomic_compare_exchange_8");

extern "C" bool SymbianAtomicCompareExchange8(
    volatile void* absl_nonnull pointer, void* absl_nonnull expected,
    TUint64 desired, int, int) {
  return SymbianRuntimeAtomic64CompareExchange(
      pointer, static_cast<TUint64*>(expected), desired);
}

extern "C" TUint64 SymbianAtomicFetchAdd8(volatile void* absl_nonnull pointer,
                                          TUint64 value,
                                          int) asm("__atomic_fetch_add_8");

extern "C" TUint64 SymbianAtomicFetchAdd8(volatile void* absl_nonnull pointer,
                                          TUint64 value, int) {
  return SymbianRuntimeAtomic64FetchAdd(pointer, value);
}

#ifdef SYMBIAN_RUNTIME_MIMALLOC
extern "C" TUint64 SymbianAtomicFetchSub8(volatile void* absl_nonnull pointer,
                                          TUint64 value,
                                          int) asm("__atomic_fetch_sub_8");

extern "C" TUint64 SymbianAtomicFetchSub8(volatile void* absl_nonnull pointer,
                                          TUint64 value, int) {
  return SymbianRuntimeAtomic64FetchAdd(pointer, TUint64{0} - value);
}
#endif

extern "C" TUint64 SymbianSyncFetchAndAdd8(
    volatile void* absl_nonnull pointer,
    TUint64 value) asm("__sync_fetch_and_add_8");

extern "C" TUint64 SymbianSyncFetchAndAdd8(volatile void* absl_nonnull pointer,
                                           TUint64 value) {
  return SymbianRuntimeAtomic64FetchAdd(pointer, value);
}

extern "C" TUint64 SymbianSyncFetchAndSub8(
    volatile void* absl_nonnull pointer,
    TUint64 value) asm("__sync_fetch_and_sub_8");

extern "C" TUint64 SymbianSyncFetchAndSub8(volatile void* absl_nonnull pointer,
                                           TUint64 value) {
  return SymbianRuntimeAtomic64FetchAdd(pointer, -value);
}

extern "C" TUint64 SymbianSyncCompareAndSwap8(
    volatile void* absl_nonnull pointer, TUint64 old_value,
    TUint64 new_value) asm("__sync_val_compare_and_swap_8");

extern "C" TUint64 SymbianSyncCompareAndSwap8(
    volatile void* absl_nonnull pointer, TUint64 old_value, TUint64 new_value) {
  SymbianRuntimeAtomic64CompareExchange(pointer, &old_value, new_value);
  return old_value;
}

extern "C" TUint64 SymbianSyncLockTestAndSet8(
    volatile void* absl_nonnull pointer,
    TUint64 value) asm("__sync_lock_test_and_set_8");

extern "C" TUint64 SymbianSyncLockTestAndSet8(
    volatile void* absl_nonnull pointer, TUint64 value) {
  return SymbianRuntimeAtomic64Exchange(pointer, value);
}

extern "C" TUint32 SymbianSyncFetchAndSub4(
    volatile void* absl_nonnull pointer,
    TUint32 value) asm("__sync_fetch_and_sub_4");

extern "C" TUint32 SymbianSyncFetchAndSub4(volatile void* absl_nonnull pointer,
                                           TUint32 value) {
  return __e32_atomic_add_ord32(pointer, -value);
}

extern "C" TUint8 SymbianSyncLockTestAndSet1(
    volatile void* absl_nonnull pointer,
    TUint8 value) asm("__sync_lock_test_and_set_1");

extern "C" TUint8 SymbianSyncLockTestAndSet1(
    volatile void* absl_nonnull pointer, TUint8 value) {
  return __e32_atomic_swp_ord8(pointer, value);
}
