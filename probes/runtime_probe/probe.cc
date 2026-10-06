#include <limits>
#include <new>
#include <string>
#include <vector>

#include <absl/base/nullability.h>

#include "abi.h"

#ifdef SYMBIAN_RUNTIME_ALLOC_BENCH
extern "C" int SymbianRuntimeAllocBench();
#endif

extern "C" const int RuntimeGotValue;
extern "C" int RuntimeGotFunction(int value);
extern "C" unsigned int RuntimeReverseBytes(unsigned int value);
extern "C" int RuntimeInitialized;
extern "C" int RuntimeBss[64];
extern "C" int* absl_nonnull RuntimeDataPointer;
extern "C" int* absl_nonnull RuntimeBssPointer;
extern "C" int (*absl_nonnull RuntimeFunctionPointer)(int);
#ifdef SYMBIAN_RUNTIME_FAST_LOCK
extern "C" int SymbianRuntimeFastLockProbe();
#endif
#ifdef SYMBIAN_RUNTIME_CHUNK
extern "C" int SymbianRuntimeChunkProbe();
#endif
#ifdef SYMBIAN_RUNTIME_TLS
extern "C" int SymbianRuntimeTlsProbe();
#endif
#ifdef SYMBIAN_RUNTIME_ATOMIC
extern "C" int SymbianRuntimeAtomicProbe();
#endif
#ifdef SYMBIAN_RUNTIME_THREAD_ATOMIC
extern "C" int SymbianRuntimeThreadAtomicProbe();
#endif
#ifdef SYMBIAN_RUNTIME_ATOMIC64
extern "C" int SymbianRuntimeAtomic64Probe();
#endif
#ifdef SYMBIAN_RUNTIME_COMPILER_RT
extern "C" int SymbianRuntimeCompilerRtProbe();
#endif
#ifdef SYMBIAN_RUNTIME_HASH_TABLE
extern "C" int SymbianRuntimeHashTableProbe();
#endif
#ifdef SYMBIAN_RUNTIME_CLOCK
extern "C" int SymbianRuntimeClockProbe();
#endif
#ifdef SYMBIAN_RUNTIME_FAST_COUNTER
extern "C" int SymbianRuntimeFastCounterProbe();
#endif
#ifdef SYMBIAN_RUNTIME_CLOCK_THREAD
extern "C" int SymbianRuntimeClockThreadProbe();
#endif
#ifdef SYMBIAN_RUNTIME_NATIVE_TIMER
extern "C" int SymbianRuntimeNativeTimerProbe();
#endif
#ifdef SYMBIAN_RUNTIME_TIMER_FUTURE
extern "C" int SymbianRuntimeTimerFutureProbe();
#endif
#ifdef SYMBIAN_RUNTIME_FIBER_CONTEXT
extern "C" int SymbianRuntimeFiberContextProbe();
#endif
#ifdef SYMBIAN_RUNTIME_FIBER_LOCKS
extern "C" int SymbianRuntimeFiberLocksProbe();
#endif
#ifdef SYMBIAN_RUNTIME_EVENT_EXECUTOR
extern "C" int SymbianRuntimeEventExecutorProbe();
#endif
#ifdef SYMBIAN_RUNTIME_DEVICE_API
extern "C" int SymbianRuntimeDeviceApiProbe();
#endif
#ifdef SYMBIAN_RUNTIME_MIMALLOC_SDK_PROBE
extern "C" int SymbianRuntimeMimallocSdkProbe();
#endif
#ifdef SYMBIAN_RUNTIME_THREAD_ERROR
extern "C" int SymbianRuntimeThreadErrorProbe();
#endif
#ifdef SYMBIAN_RUNTIME_DIRECT_ATOMIC64_DIAGNOSTIC
extern "C" int SymbianRuntimeNativeAtomic64Probe();
#endif
#ifdef SYMBIAN_RUNTIME_OWNERSHIP
extern "C" int SymbianRuntimeOwnershipProbe();
#endif
#ifdef SYMBIAN_RUNTIME_STD_THREAD
extern "C" int SymbianRuntimeStdThreadProbe();
#endif
#ifdef SYMBIAN_RUNTIME_A11_STACKLESS
extern "C" int SymbianRuntimeA11StacklessProbe();
#endif
#ifdef SYMBIAN_RUNTIME_VARARGS
int SymbianRuntimeVarargsProbe();
#endif
#ifdef SYMBIAN_RUNTIME_SYSTEM_ERROR
int SymbianRuntimeSystemErrorProbe();
#endif
#ifdef SYMBIAN_RUNTIME_LOCALE_STREAM
int SymbianRuntimeLocaleStreamProbe();
int SymbianRuntimeLocaleApiProbe();
#endif
#ifdef SYMBIAN_RUNTIME_IMPORT_POINTER
extern void* absl_nonnull (*absl_nonnull RuntimeImportedMemmove)(
    void* absl_nonnull, const void* absl_nonnull, size_t);
#endif
#ifdef SYMBIAN_RUNTIME_LONG_THUNK
extern "C" unsigned int RuntimeArmThunkCall(unsigned int value);
#endif
#ifdef SYMBIAN_RUNTIME_EXCEPTIONS
extern "C" int SymbianRuntimeExceptionProbe();
#endif
#ifdef SYMBIAN_RUNTIME_GLOBAL_LIFETIME
extern "C" int RuntimeCheckInitializers();
#endif

int main() {
#ifdef SYMBIAN_RUNTIME_ALLOC_BENCH
  return SymbianRuntimeAllocBench();
#endif
#ifdef SYMBIAN_RUNTIME_DEVICE_API
  if (int result = SymbianRuntimeDeviceApiProbe(); result != 0) {
    return result;
  }
#endif
#ifdef SYMBIAN_RUNTIME_EVENT_EXECUTOR
  if (int result = SymbianRuntimeEventExecutorProbe(); result != 0) {
    return result;
  }
#endif
#ifdef SYMBIAN_RUNTIME_MIMALLOC_SDK_PROBE
  if (int result = SymbianRuntimeMimallocSdkProbe(); result != 0) {
    return result;
  }
#endif
#ifdef SYMBIAN_RUNTIME_FIBER_LOCKS
  if (int result = SymbianRuntimeFiberLocksProbe(); result != 0) {
    return result;
  }
#endif
#ifdef SYMBIAN_RUNTIME_FIBER_CONTEXT
  if (int result = SymbianRuntimeFiberContextProbe(); result != 0) {
    return result;
  }
#endif
#ifdef SYMBIAN_RUNTIME_TLS
  if (int result = SymbianRuntimeTlsProbe(); result != 0) {
    return result;
  }
#endif
#ifdef SYMBIAN_RUNTIME_TIMER_FUTURE
  if (int result = SymbianRuntimeTimerFutureProbe(); result != 0) {
    return result;
  }
#endif
#ifdef SYMBIAN_RUNTIME_NATIVE_TIMER
  if (int result = SymbianRuntimeNativeTimerProbe(); result != 0) {
    return result;
  }
#endif
#ifdef SYMBIAN_RUNTIME_CHUNK
  if (int result = SymbianRuntimeChunkProbe(); result != 0) {
    return result;
  }
#endif
#ifdef SYMBIAN_RUNTIME_FAST_LOCK
  if (int result = SymbianRuntimeFastLockProbe(); result != 0) {
    return result;
  }
#endif
#ifdef SYMBIAN_RUNTIME_ATOMIC
  if (int result = SymbianRuntimeAtomicProbe(); result != 0) {
    return result;
  }
#endif
#ifdef SYMBIAN_RUNTIME_THREAD_ATOMIC
  if (int result = SymbianRuntimeThreadAtomicProbe(); result != 0) {
    return result;
  }
#endif
#ifdef SYMBIAN_RUNTIME_ATOMIC64
  if (int result = SymbianRuntimeAtomic64Probe(); result != 0) {
    return result;
  }
#endif
#ifdef SYMBIAN_RUNTIME_COMPILER_RT
  if (int result = SymbianRuntimeCompilerRtProbe(); result != 0) {
    return result;
  }
#endif
#ifdef SYMBIAN_RUNTIME_HASH_TABLE
  if (int result = SymbianRuntimeHashTableProbe(); result != 0) {
    return result;
  }
#endif
#ifdef SYMBIAN_RUNTIME_CLOCK
  if (int result = SymbianRuntimeClockProbe(); result != 0) {
    return result;
  }
#endif
#ifdef SYMBIAN_RUNTIME_FAST_COUNTER
  if (int result = SymbianRuntimeFastCounterProbe(); result != 0) {
    return result;
  }
#endif
#ifdef SYMBIAN_RUNTIME_CLOCK_THREAD
  if (int result = SymbianRuntimeClockThreadProbe(); result != 0) {
    return result;
  }
#endif
#ifdef SYMBIAN_RUNTIME_THREAD_ERROR
  if (int result = SymbianRuntimeThreadErrorProbe(); result != 0) {
    return result;
  }
#endif
#ifdef SYMBIAN_RUNTIME_DIRECT_ATOMIC64_DIAGNOSTIC
  if (int result = SymbianRuntimeNativeAtomic64Probe(); result != 0) {
    return result;
  }
#endif
#ifdef SYMBIAN_RUNTIME_OWNERSHIP
  if (int result = SymbianRuntimeOwnershipProbe(); result != 0) {
    return result;
  }
#endif
#ifdef SYMBIAN_RUNTIME_STD_THREAD
  if (int result = SymbianRuntimeStdThreadProbe(); result != 0) {
    return result;
  }
#endif
#ifdef SYMBIAN_RUNTIME_A11_STACKLESS
  if (int result = SymbianRuntimeA11StacklessProbe(); result != 0) {
    return result;
  }
#endif
#ifdef SYMBIAN_RUNTIME_VARARGS
  if (int result = SymbianRuntimeVarargsProbe(); result != 0) {
    return result;
  }
#endif
#ifdef SYMBIAN_RUNTIME_SYSTEM_ERROR
  if (int result = SymbianRuntimeSystemErrorProbe(); result != 0) {
    return result;
  }
#endif
#ifdef SYMBIAN_RUNTIME_LOCALE_STREAM
  if (int result = SymbianRuntimeLocaleStreamProbe(); result != 0) {
    return result;
  }
  if (int result = SymbianRuntimeLocaleApiProbe(); result != 0) {
    return result;
  }
#endif
#ifdef SYMBIAN_RUNTIME_IMPORT_POINTER
  char source[] = "stream";
  char target[sizeof(source)] = {};
  if (RuntimeImportedMemmove(target, source, sizeof(source)) != target) {
    return -192;
  }
  for (size_t index = 0; index < sizeof(source); ++index) {
    if (target[index] != source[index]) {
      return -192;
    }
  }
#endif
#ifdef SYMBIAN_RUNTIME_LONG_THUNK
  if (RuntimeArmThunkCall(0x11223344) != 0x44332211) {
    return -193;
  }
#endif
#ifdef SYMBIAN_RUNTIME_EXCEPTIONS
  if (int result = SymbianRuntimeExceptionProbe(); result != 0) {
    return result;
  }
#endif
#ifdef SYMBIAN_RUNTIME_GLOBAL_LIFETIME
  if (int result = RuntimeCheckInitializers(); result != 0) {
    return result;
  }
#endif
#ifdef SYMBIAN_RUNTIME_FAIL_ALLOCATION
  // The image has a 1 MiB maximum heap. This representable request must fail
  // through the real SDK allocator, rather than the TInt range guard.
  volatile size_t size = 4 * 1024 * 1024;
  void* absl_nonnull pointer = ::operator new(size);
  ::operator delete(pointer);
  return -2;
#else
  // Volatile operands force the real ARM helpers, not constant folding.
  volatile int numerator = -2026;
  volatile int denominator = 100;
  if (numerator / denominator != -20 || numerator % denominator != -26) {
    return -109;
  }
  volatile unsigned int largest = 0xffffffffU;
  volatile unsigned int divisor = 17;
  if (largest / divisor != 252645135U || largest % divisor != 0) {
    return -110;
  }
  // Force the ARM EABI 64-bit quotient/remainder helpers from compiler-rt.
  volatile unsigned long long wide_unsigned = 0xfedcba9876543210ULL;
  volatile unsigned long long wide_divisor = 0x12345ULL;
  const unsigned long long wide_quotient = wide_unsigned / wide_divisor;
  const unsigned long long wide_remainder = wide_unsigned % wide_divisor;
  volatile long long wide_signed = -0x7123456789abcdefLL;
  volatile long long signed_divisor = 0x12345LL;
  const long long signed_quotient = wide_signed / signed_divisor;
  const long long signed_remainder = wide_signed % signed_divisor;
#ifdef SYMBIAN_RUNTIME_CHANGED_WIDE
  if (wide_remainder != 0x10a50ULL) {
    return -126;
  }
#else
  if (wide_quotient != 0xe0004fa01c4dULL || wide_remainder != 0x10a4fULL ||
      signed_quotient != -109333280513168LL || signed_remainder != -43807LL) {
    return -126;
  }
#endif
#ifdef SYMBIAN_RUNTIME_DATA
  if (RuntimeInitialized != 2026) {
    return -115;
  }
  if (RuntimeDataPointer != &RuntimeInitialized ||
      RuntimeBssPointer != RuntimeBss ||
      RuntimeFunctionPointer != &RuntimeGotFunction) {
    return -116;
  }
  for (int value : RuntimeBss) {
    if (value != 0) {
      return -117;
    }
  }
  for (int i = 0; i < 64; ++i) {
    RuntimeBssPointer[i] = i + RuntimeInitialized;
  }
  ++*RuntimeDataPointer;
  if (RuntimeFunctionPointer(RuntimeInitialized) != 2835 ||
      RuntimeBss[63] != 2089 || RuntimeInitialized != 2027) {
    return -118;
  }
#endif
  volatile unsigned int endian_input = 0x11223344;
  // ARMv6 emits REV; ARMv5T uses a software sequence. Both must compute the
  // same result. Disassembly acceptance checks the actual v6 opcode separately.
  if (RuntimeReverseBytes(endian_input) != 0x44332211) {
    return -119;
  }
  const int before = SymbianRuntimeAllocationCells();
  for (int round = 0; round < 20; ++round) {
    std::string text(80, 'a');  // Must exceed the small-string inline capacity.
    text.append(" Symbian C++20");
    text.insert(0, "Nokia ");
    text.erase(0, 6);
    if (text.size() != 94 || text.substr(81) != "Symbian C++20") {
      return -101;
    }
    std::vector<int> values;
    for (int i = 1; i <= 1024; ++i) {
      values.push_back(i);
    }
    int sum = 0;
    for (int value : values) {
      sum += value;
    }
    if (sum != 524800 || values.size() != 1024) {
      return -102;
    }
  }
  if (SymbianRuntimeAllocationCells() != before) {
    return -103;
  }
#ifdef SYMBIAN_RUNTIME_GLOBAL_NOTHROW
  // Actually dereference an external object and call a Thumb function through
  // the GOT. The empty nothrow tag alone cannot detect a bad object address.
  const volatile int* absl_nonnull object = &RuntimeGotValue;
  if (*object != 2026) {
    return -113;
  }
  int (*absl_nonnull volatile callback)(int) = &RuntimeGotFunction;
  if (callback(*object) != 2834) {
    return -114;
  }
  const auto& nothrow = std::nothrow;
#else
  const std::nothrow_t nothrow{};
#endif
  void* absl_nullable zero = ::operator new(0, nothrow);
  if (zero == nullptr) {
    return -104;
  }
  if (reinterpret_cast<size_t>(zero) % alignof(std::max_align_t) != 0) {
    return -107;
  }
  ::operator delete(zero);
  ::operator delete(nullptr);
  const auto alignment = static_cast<std::align_val_t>(512);
  void* absl_nullable aligned = ::operator new(257, alignment, nothrow);
  if (aligned == nullptr || reinterpret_cast<size_t>(aligned) % 512 != 0) {
    return -111;
  }
  ::operator delete(aligned, size_t{257}, alignment);
  volatile size_t oversized = std::numeric_limits<size_t>::max();
  if (::operator new(oversized, alignment, nothrow) != nullptr) {
    return -112;
  }
  if (::operator new(oversized, nothrow) != nullptr) {
    return -105;
  }
  volatile size_t exhausted = 4 * 1024 * 1024;
  void* absl_nullable large = ::operator new(exhausted, nothrow);
#ifdef SYMBIAN_RUNTIME_MIMALLOC
  if (large == nullptr) {
    return -108;
  }
  ::operator delete(large);
#else
  if (large != nullptr) {
    return -108;
  }
#endif
  return SymbianRuntimeAllocationCells() == before ? 0 : -106;
#endif
}
