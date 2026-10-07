// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_RUNTIME_MATH_LEGACY_H_
#define SYMBIAN_RUNTIME_MATH_LEGACY_H_

#include <bit>
#include <cstdint>

#include <absl/base/nullability.h>

namespace symbian::runtime::math {

inline float Ceil(float value) {
  const std::uint32_t bits = std::bit_cast<std::uint32_t>(value);
  const int exponent = static_cast<int>((bits >> 23) & 0xff) - 127;
  if (exponent >= 23) {
    return value;
  }
  if (exponent < 0) {
    if ((bits & 0x7fffffffU) == 0) {
      return value;
    }
    return (bits & 0x80000000U) != 0 ? -0.0f : 1.0f;
  }
  const std::uint32_t fraction = (1U << (23 - exponent)) - 1;
  if ((bits & fraction) == 0) {
    return value;
  }
  std::uint32_t integral = bits & ~fraction;
  if ((bits & 0x80000000U) == 0) {
    integral += 1U << (23 - exponent);
  }
  return std::bit_cast<float>(integral);
}

inline double Frexp(double value, int* absl_nonnull exponent) {
  std::uint64_t bits = std::bit_cast<std::uint64_t>(value);
  int biased = static_cast<int>((bits >> 52) & 0x7ff);
  *exponent = 0;
  if (biased == 0x7ff || (bits & 0x7fffffffffffffffULL) == 0) {
    return value;
  }
  if (biased == 0) {
    value *= 0x1p54;
    bits = std::bit_cast<std::uint64_t>(value);
    biased = static_cast<int>((bits >> 52) & 0x7ff);
    *exponent -= 54;
  }
  *exponent += biased - 1022;
  bits = (bits & 0x800fffffffffffffULL) | (std::uint64_t{1022} << 52);
  return std::bit_cast<double>(bits);
}

inline double Ldexp(double value, int exponent) {
  std::uint64_t bits = std::bit_cast<std::uint64_t>(value);
  if ((bits & 0x7fffffffffffffffULL) == 0 ||
      (bits & 0x7ff0000000000000ULL) == 0x7ff0000000000000ULL ||
      exponent == 0) {
    return value;
  }
  int biased = static_cast<int>((bits >> 52) & 0x7ff);
  if (biased == 0) {
    value *= 0x1p54;
    bits = std::bit_cast<std::uint64_t>(value);
    biased = static_cast<int>((bits >> 52) & 0x7ff) - 54;
  }
  if (exponent > 50000) {
    exponent = 50000;
  } else if (exponent < -50000) {
    exponent = -50000;
  }
  const int result_exponent = biased + exponent;
  if (result_exponent >= 0x7ff) {
    return std::bit_cast<double>((bits & 0x8000000000000000ULL) |
                                 0x7ff0000000000000ULL);
  }
  if (result_exponent > 0) {
    bits = (bits & 0x800fffffffffffffULL) |
           (static_cast<std::uint64_t>(result_exponent) << 52);
    return std::bit_cast<double>(bits);
  }
  if (result_exponent <= -53) {
    return std::bit_cast<double>(bits & 0x8000000000000000ULL);
  }
  const int shift = 1 - result_exponent;
  const std::uint64_t significand =
      (bits & 0x000fffffffffffffULL) | 0x0010000000000000ULL;
  const std::uint64_t quotient = significand >> shift;
  const std::uint64_t remainder =
      significand & ((std::uint64_t{1} << shift) - 1);
  const std::uint64_t halfway = std::uint64_t{1} << (shift - 1);
  const std::uint64_t rounded =
      quotient +
      (remainder > halfway || (remainder == halfway && (quotient & 1)));
  return std::bit_cast<double>((bits & 0x8000000000000000ULL) | rounded);
}

inline float Scalbn(float value, int exponent) {
  std::uint32_t bits = std::bit_cast<std::uint32_t>(value);
  if ((bits & 0x7fffffffU) == 0 || (bits & 0x7f800000U) == 0x7f800000U ||
      exponent == 0) {
    return value;
  }
  int biased = static_cast<int>((bits >> 23) & 0xff);
  if (biased == 0) {
    value *= 0x1p25f;
    bits = std::bit_cast<std::uint32_t>(value);
    biased = static_cast<int>((bits >> 23) & 0xff) - 25;
  }
  if (exponent > 50000) {
    exponent = 50000;
  } else if (exponent < -50000) {
    exponent = -50000;
  }
  const int result_exponent = biased + exponent;
  if (result_exponent >= 0xff) {
    return std::bit_cast<float>((bits & 0x80000000U) | 0x7f800000U);
  }
  if (result_exponent > 0) {
    bits = (bits & 0x807fffffU) |
           (static_cast<std::uint32_t>(result_exponent) << 23);
    return std::bit_cast<float>(bits);
  }
  if (result_exponent <= -24) {
    return std::bit_cast<float>(bits & 0x80000000U);
  }
  const int shift = 1 - result_exponent;
  const std::uint32_t significand = (bits & 0x007fffffU) | 0x00800000U;
  const std::uint32_t quotient = significand >> shift;
  const std::uint32_t remainder =
      significand & ((std::uint32_t{1} << shift) - 1);
  const std::uint32_t halfway = std::uint32_t{1} << (shift - 1);
  const std::uint32_t rounded =
      quotient +
      (remainder > halfway || (remainder == halfway && (quotient & 1)));
  return std::bit_cast<float>((bits & 0x80000000U) | rounded);
}

inline double Modf(double value, double* absl_nonnull integral) {
  const std::uint64_t bits = std::bit_cast<std::uint64_t>(value);
  const std::uint64_t sign = bits & 0x8000000000000000ULL;
  const int exponent = static_cast<int>((bits >> 52) & 0x7ff) - 1023;
  if (exponent < 0) {
    *integral = std::bit_cast<double>(sign);
    return value;
  }
  if (exponent >= 52) {
    *integral = value;
    return (bits & 0x7fffffffffffffffULL) > 0x7ff0000000000000ULL
               ? value
               : std::bit_cast<double>(sign);
  }
  const std::uint64_t fraction = (std::uint64_t{1} << (52 - exponent)) - 1;
  *integral = std::bit_cast<double>(bits & ~fraction);
  if ((bits & fraction) == 0) {
    return std::bit_cast<double>(sign);
  }
  return value - *integral;
}

inline float Nextafter(float value, float toward) {
  std::uint32_t bits = std::bit_cast<std::uint32_t>(value);
  const std::uint32_t target = std::bit_cast<std::uint32_t>(toward);
  if ((bits & 0x7fffffffU) > 0x7f800000U ||
      (target & 0x7fffffffU) > 0x7f800000U) {
    return value + toward;
  }
  if (value == toward) {
    return toward;
  }
  if ((bits & 0x7fffffffU) == 0) {
    return std::bit_cast<float>((target & 0x80000000U) | 1U);
  }
  if ((value < toward) == (value > 0)) {
    ++bits;
  } else {
    --bits;
  }
  return std::bit_cast<float>(bits);
}

inline double Round(double value) {
  const std::uint64_t bits = std::bit_cast<std::uint64_t>(value);
  const int exponent = static_cast<int>((bits >> 52) & 0x7ff) - 1023;
  if (exponent >= 52) {
    return value;
  }
  const std::uint64_t sign = bits & 0x8000000000000000ULL;
  if (exponent < -1) {
    return std::bit_cast<double>(sign);
  }
  if (exponent == -1) {
    return std::bit_cast<double>(sign | 0x3ff0000000000000ULL);
  }
  const std::uint64_t fraction = (std::uint64_t{1} << (52 - exponent)) - 1;
  const std::uint64_t half = std::uint64_t{1} << (51 - exponent);
  std::uint64_t rounded = bits & ~fraction;
  if ((bits & fraction) >= half) {
    rounded += std::uint64_t{1} << (52 - exponent);
  }
  return std::bit_cast<double>(rounded);
}

}  // namespace symbian::runtime::math

#endif  // SYMBIAN_RUNTIME_MATH_LEGACY_H_
