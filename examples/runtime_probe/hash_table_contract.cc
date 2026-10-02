#include <bit>
#include <cstdint>
#include <string>
#include <unordered_set>

#include <math.h>

#include "abi.h"

extern "C" int SymbianRuntimeHashTableProbe() {
  float (*volatile scale)(float, int) = &ldexpf;
  if (scale(0.75f, 3) != 6.0f || scale(8.0f, -2) != 2.0f) {
    return -232;
  }
  long double (*volatile wide_scale)(long double, int) = &ldexpl;
  double (*volatile make_nan)(const char*) = &nan;
  float (*volatile make_nanf)(const char*) = &nanf;
  if (wide_scale(0.75L, 3) != 6.0L ||
      std::bit_cast<std::uint64_t>(make_nan("0x123")) !=
          0x7ff8000000000123ULL ||
      std::bit_cast<std::uint32_t>(make_nanf("17")) != 0x7fc00011U ||
      std::bit_cast<std::uint64_t>(make_nan("bad")) != 0x7ff8000000000000ULL) {
    return -234;
  }
  const int before = SymbianRuntimeAllocationCells();
  {
    std::unordered_set<int> values;
    for (int value = 0; value < 300; ++value) {
      if (!values.insert(value * 17).second) {
        return -224;
      }
    }
    if (values.size() != 300 || values.insert(17).second) {
      return -225;
    }
    values.reserve(700);
    if (values.bucket_count() < 700) {
      return -226;
    }
    for (int value = 0; value < 300; ++value) {
      if (values.find(value * 17) == values.end()) {
        return -227;
      }
      if (value < 100 && values.erase(value * 17) != 1) {
        return -228;
      }
    }
    if (values.size() != 200) {
      return -229;
    }
    std::unordered_set<std::string> labels;
    labels.insert("Symbian");
    labels.insert("Abseil");
    if (labels.find("Abseil") == labels.end() ||
        labels.find("missing") != labels.end()) {
      return -233;
    }
  }
  if (SymbianRuntimeAllocationCells() != before) {
    return -230;
  }
#ifdef SYMBIAN_RUNTIME_CHANGED_HASH_TABLE
  return -231;
#else
  return 0;
#endif
}
