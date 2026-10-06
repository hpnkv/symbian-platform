// SPDX-License-Identifier: Apache-2.0
#include <array>

#include "gtest/gtest.h"
#include "symbian/entropy/veneer.h"

namespace symbian::entropy {
namespace {
TEST(SecureRandomVeneerTest, OriginalRomWrappersSelectTheSameNativeCall) {
  // Original ROM instructions: C7/E6 use 0x109; 808 Belle uses 0x10a.
  const std::array<std::uint16_t, 4> c7_random{0xb510, 0xf7f4, 0xefea, 0xbd10};
  const std::array<std::uint16_t, 6> c7_random_l{0xb510, 0xf7f4, 0xefe6,
                                                 0xf009, 0xfee8, 0xbd10};
  EXPECT_EQ(SecureRandomVeneer(0x8048562f, c7_random, false), 0x8047a608);
  EXPECT_EQ(SecureRandomVeneer(0x80485637, c7_random_l, true), 0x8047a608);
  const std::array<std::uint16_t, 4> belle_random{0xb510, 0xf7f4, 0xefe2,
                                                  0xbd10};
  const std::array<std::uint16_t, 6> belle_random_l{0xb510, 0xf7f4, 0xefde,
                                                    0xf009, 0xfef4, 0xbd10};
  EXPECT_EQ(SecureRandomVeneer(0x804cace7, belle_random, false), 0x804bfcb0);
  EXPECT_EQ(SecureRandomVeneer(0x804cacef, belle_random_l, true), 0x804bfcb0);
  EXPECT_TRUE(IsSecureRandomVeneer(
      std::array<std::uint32_t, 2>{0xef000109, 0xe12fff1e}));
  EXPECT_TRUE(IsSecureRandomVeneer(
      std::array<std::uint32_t, 2>{0xef00010a, 0xe12fff1e}));
}

TEST(SecureRandomVeneerTest, RejectsMissingTruncatedAndUnknownWrappers) {
  std::array<std::uint16_t, 4> code{0xb510, 0xf7f4, 0xefea, 0xbd10};
  EXPECT_EQ(SecureRandomVeneer(0, {}, false), 0);
  EXPECT_EQ(SecureRandomVeneer(0x8048562e, code, false), 0);
  EXPECT_EQ(SecureRandomVeneer(0x8048562f, code, true), 0);
  for (std::size_t length = 0; length < code.size(); ++length) {
    EXPECT_EQ(
        SecureRandomVeneer(0x8048562f, std::span(code).first(length), false),
        0);
  }
  for (std::size_t i = 0; i < code.size(); ++i) {
    auto altered = code;
    altered[i] = 0;
    EXPECT_EQ(SecureRandomVeneer(0x8048562f, altered, false), 0);
  }
  // BLX must preserve ARM alignment and use the ARM, rather than Thumb, form.
  code[2] |= 1;
  EXPECT_EQ(SecureRandomVeneer(0x8048562f, code, false), 0);
  EXPECT_FALSE(IsSecureRandomVeneer({}));
  EXPECT_FALSE(IsSecureRandomVeneer(
      std::array<std::uint32_t, 2>{0xea000109, 0xe12fff1e}));
  EXPECT_FALSE(IsSecureRandomVeneer(
      std::array<std::uint32_t, 2>{0xef000109, 0xe12fff10}));
}

TEST(SecureRandomVeneerTest, RejectsAddressOverflowAndWrongLeaveCall) {
  const std::array<std::uint16_t, 4> forward{0xb510, 0xf000, 0xe802, 0xbd10};
  EXPECT_EQ(SecureRandomVeneer(0xfffffffd, forward, false), 0);
  const std::array<std::uint16_t, 6> bad_leave{0xb510, 0xf7f4, 0xefe6,
                                               0xf009, 0xeee8, 0xbd10};
  EXPECT_EQ(SecureRandomVeneer(0x80485637, bad_leave, true), 0);
}
}  // namespace
}  // namespace symbian::entropy
