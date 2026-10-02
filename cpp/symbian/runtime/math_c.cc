#include <bit>
#include <cstdint>
#include <cstdlib>

#include <math.h>

namespace {

std::uint64_t NanPayload(const char* tag) {
  if (tag == nullptr || *tag == '\0') {
    return 0;
  }
  char* end = nullptr;
  const unsigned long long value = std::strtoull(tag, &end, 0);
  return end != tag && *end == '\0' ? value : 0;
}

}  // namespace

extern "C" float ldexpf(float value, int exponent) {
  return scalbnf(value, exponent);
}

extern "C" long double ldexpl(long double value, int exponent) {
  static_assert(sizeof(long double) == sizeof(double));
  return ldexp(static_cast<double>(value), exponent);
}

extern "C" double nan(const char* tag) {
  constexpr std::uint64_t kQuietNan = 0x7ff8000000000000ULL;
  constexpr std::uint64_t kPayloadMask = 0x0007ffffffffffffULL;
  return std::bit_cast<double>(kQuietNan | (NanPayload(tag) & kPayloadMask));
}

extern "C" float nanf(const char* tag) {
  constexpr std::uint32_t kQuietNan = 0x7fc00000U;
  constexpr std::uint32_t kPayloadMask = 0x003fffffU;
  return std::bit_cast<float>(
      kQuietNan | static_cast<std::uint32_t>(NanPayload(tag) & kPayloadMask));
}
