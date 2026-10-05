#include <bit>
#include <cstdint>

// Calls the original pinned compiler-rt ARM EABI entry points. This test
// translation unit is an ABI probe, not an application-facing SDK interface.
extern "C" std::uint32_t __aeabi_f2uiz(float value);
extern "C" void __aeabi_memcpy8(void* destination, const void* source,
                                std::uint32_t size);
extern "C" void __aeabi_memmove8(void* destination, const void* source,
                                 std::uint32_t size);

extern "C" int SymbianRuntimeCompilerRtProbe() {
  alignas(8) std::uint64_t source[] = {
      0x123456789ABCDEF0ULL, 0x0FEDCBA987654321ULL, 0xA55AA55A11223344ULL};
  alignas(8) std::uint64_t copied[] = {0, 0, 0};
  __aeabi_memcpy8(copied, source, sizeof(source));
  if (copied[0] != source[0] || copied[1] != source[1] ||
      copied[2] != source[2]) {
    return -220;
  }
  alignas(8) std::uint64_t shifted[] = {11, 22, 33, 44};
  __aeabi_memmove8(shifted + 1, shifted, 3 * sizeof(std::uint64_t));
  if (shifted[0] != 11 || shifted[1] != 11 || shifted[2] != 22 ||
      shifted[3] != 33) {
    return -227;
  }

  volatile float fraction = 12.75f;
  volatile float negative = -1.0f;
  volatile float overflow = 4294967296.0f;
  volatile float nan = std::bit_cast<float>(0x7FC00000U);
  if (__aeabi_f2uiz(fraction) != 12 || __aeabi_f2uiz(negative) != 0 ||
      __aeabi_f2uiz(overflow) != 0xFFFFFFFFU || __aeabi_f2uiz(nan) != 0) {
    return -221;
  }
  volatile std::uint64_t float_source = 16777216;
  if (fraction - 2.5f != 10.25f || static_cast<int>(fraction) != 12 ||
      static_cast<std::uint64_t>(fraction) != 12 ||
      static_cast<float>(float_source) != 16777216.0f) {
    return -226;
  }

  volatile double left = 7.5;
  volatile double right = 2.0;
  if (left + right != 9.5 || left - right != 5.5 || left * right != 15.0 ||
      left / right != 3.75 || !(left > right) || !(right < left) ||
      left == right) {
    return -223;
  }
  volatile std::int64_t signed_value = -123456789;
  volatile std::uint64_t unsigned_value = 123456789;
  if (static_cast<std::int64_t>(static_cast<double>(signed_value)) !=
          signed_value ||
      static_cast<std::uint64_t>(static_cast<double>(unsigned_value)) !=
          unsigned_value ||
      static_cast<double>(fraction) != 12.75 ||
      static_cast<float>(left) != 7.5f) {
    return -224;
  }
  volatile double double_nan = std::bit_cast<double>(0x7FF8000000000000ULL);
  if (double_nan == double_nan || double_nan < left || double_nan > left) {
    return -225;
  }
#ifdef SYMBIAN_RUNTIME_CHANGED_COMPILER_RT
  return copied[2] == source[2] + 1 ? 0 : -222;
#else
  return 0;
#endif
}
