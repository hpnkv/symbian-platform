#include <e32atomics.h>

// Direct original-SDK oracle for the compiler ABI adaptation. Kept behind an
// opt-in diagnostic mode until every requested native operation is verified.
extern "C" int SymbianRuntimeNativeAtomic64Probe() {
  alignas(8) volatile TUint64 value = 0x100000002ULL;
  if (__e32_atomic_load_acq64(&value) != 0x100000002ULL) {
    return -216;
  }
  const TUint64 old = __e32_atomic_add_ord64(&value, 0x100000001ULL);
  if (__e32_atomic_load_acq64(&value) != 0x200000003ULL) {
    return -218;
  }
  if (old != 0x100000002ULL) {
    return -217;
  }
  TUint64 expected = 0x200000003ULL;
  if (!__e32_atomic_cas_ord64(&value, &expected, 0x300000004ULL) ||
      __e32_atomic_load_acq64(&value) != 0x300000004ULL) {
    return -219;
  }
  if (__e32_atomic_swp_ord64(&value, 0x400000005ULL) != 0x300000004ULL ||
      __e32_atomic_load_acq64(&value) != 0x400000005ULL) {
    return -220;
  }
  return 0;
}
