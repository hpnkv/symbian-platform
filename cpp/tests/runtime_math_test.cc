// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include <bit>
#include <cstdint>
#include <limits>
#include <random>

#include <cmath>
#include <gtest/gtest.h>

#include "symbian/runtime/math_legacy.h"

namespace {

TEST(RuntimeMathTest, MatchesCommonFiniteOperations) {
  std::mt19937_64 random(0x808);
  for (int sample = 0; sample < 100000; ++sample) {
    const double value =
        std::bit_cast<double>(random() & 0x7fffffffffffffffULL);
    const float small =
        std::bit_cast<float>(static_cast<std::uint32_t>(random()));
    const float toward =
        std::bit_cast<float>(static_cast<std::uint32_t>(random()));
    const int exponent = static_cast<int>(random() % 201) - 100;
    if (std::isfinite(value)) {
      int actual_exponent = 0;
      int expected_exponent = 0;
      EXPECT_EQ(
          std::bit_cast<std::uint64_t>(
              symbian::runtime::math::Frexp(value, &actual_exponent)),
          std::bit_cast<std::uint64_t>(std::frexp(value, &expected_exponent)));
      EXPECT_EQ(actual_exponent, expected_exponent);
      EXPECT_EQ(std::bit_cast<std::uint64_t>(
                    symbian::runtime::math::Ldexp(value, exponent)),
                std::bit_cast<std::uint64_t>(std::ldexp(value, exponent)));
      double actual_integral = 0;
      double expected_integral = 0;
      EXPECT_EQ(
          std::bit_cast<std::uint64_t>(
              symbian::runtime::math::Modf(value, &actual_integral)),
          std::bit_cast<std::uint64_t>(std::modf(value, &expected_integral)));
      EXPECT_EQ(std::bit_cast<std::uint64_t>(actual_integral),
                std::bit_cast<std::uint64_t>(expected_integral));
      EXPECT_EQ(
          std::bit_cast<std::uint64_t>(symbian::runtime::math::Round(value)),
          std::bit_cast<std::uint64_t>(std::round(value)));
    }
    if (std::isfinite(small)) {
      EXPECT_EQ(
          std::bit_cast<std::uint32_t>(symbian::runtime::math::Ceil(small)),
          std::bit_cast<std::uint32_t>(std::ceil(small)));
      EXPECT_EQ(std::bit_cast<std::uint32_t>(
                    symbian::runtime::math::Scalbn(small, exponent)),
                std::bit_cast<std::uint32_t>(std::scalbn(small, exponent)));
    }
    if (std::isfinite(small) && std::isfinite(toward)) {
      EXPECT_EQ(std::bit_cast<std::uint32_t>(
                    symbian::runtime::math::Nextafter(small, toward)),
                std::bit_cast<std::uint32_t>(std::nextafter(small, toward)));
    }
  }
}

TEST(RuntimeMathTest, RoundsSubnormalTiesToEven) {
  constexpr double kSmallest = std::numeric_limits<double>::denorm_min();
  EXPECT_EQ(symbian::runtime::math::Ldexp(3.0, -1075), 2.0 * kSmallest);
  EXPECT_EQ(symbian::runtime::math::Ldexp(1.0, -1075), 0.0);
  constexpr float kSmallestFloat = std::numeric_limits<float>::denorm_min();
  EXPECT_EQ(symbian::runtime::math::Scalbn(3.0f, -150), 2.0f * kSmallestFloat);
  EXPECT_EQ(symbian::runtime::math::Scalbn(1.0f, -150), 0.0f);
  EXPECT_TRUE(std::signbit(symbian::runtime::math::Ceil(-0.25f)));
  EXPECT_TRUE(std::signbit(symbian::runtime::math::Round(-0.25)));
}

}  // namespace
