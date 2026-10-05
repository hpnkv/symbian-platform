#include <cstdint>
#ifdef SYMBIAN_RUNTIME_ALLOC_BENCH_PTHREAD
#include <pthread.h>
#endif

#include "abi.h"

#ifdef SYMBIAN_RUNTIME_ALLOC_BENCH_ATOMIC64
extern "C" std::uint64_t SymbianRuntimeAtomic64FetchAdd(volatile void*,
                                                        std::uint64_t);
#endif

namespace {
int Burst(int rounds) {
  void* pointers[128];
  for (int round = 0; round < rounds; ++round) {
    for (void*& pointer : pointers) {
      pointer = SymbianRuntimeAllocate(64);
      if (pointer == nullptr) {
        return -169;
      }
      static_cast<unsigned char*>(pointer)[63] = 0x5a;
    }
    for (void* pointer : pointers) {
      if (static_cast<unsigned char*>(pointer)[63] != 0x5a) {
        return -170;
      }
      SymbianRuntimeFree(pointer);
    }
  }
  return 0;
}
}  // namespace

extern "C" int SymbianRuntimeAllocBench() {
#ifdef SYMBIAN_RUNTIME_ALLOC_BENCH_PTHREAD
  pthread_key_t key;
  if (pthread_key_create(&key, nullptr) != 0 ||
      pthread_setspecific(key, reinterpret_cast<void*>(4)) != 0) {
    return -172;
  }
  std::uintptr_t result = 0;
  const std::uint32_t start = SymbianRuntimeFastCounter();
  for (int i = 0; i < 65536; ++i) {
    result += reinterpret_cast<std::uintptr_t>(pthread_getspecific(key));
  }
  const int elapsed = static_cast<int>(SymbianRuntimeFastCounter() - start);
  pthread_key_delete(key);
  return result == 65536 * 4 ? elapsed : -173;
#elif defined(SYMBIAN_RUNTIME_ALLOC_BENCH_ATOMIC64)
  volatile std::uint64_t value = 0;
  const std::uint32_t start = SymbianRuntimeFastCounter();
  for (int i = 0; i < 65536; ++i) {
    SymbianRuntimeAtomic64FetchAdd(&value, 1);
  }
  const int elapsed = static_cast<int>(SymbianRuntimeFastCounter() - start);
  return value == 65536 ? elapsed : -174;
#else
  if (int result = Burst(2); result != 0) {
    return result;
  }
  const std::uint32_t start = SymbianRuntimeFastCounter();
  if (int result = Burst(256); result != 0) {
    return result;
  }
  return static_cast<int>(SymbianRuntimeFastCounter() - start);
#endif
}
