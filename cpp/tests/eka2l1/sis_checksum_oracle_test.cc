// Independent SIS oracle using Nokia's unchanged EPL-1.0 CRC implementation.

#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>

#include <absl/base/nullability.h>
#include <gtest/gtest.h>

#include "checksum.h"
#include "symbian/analysis/bytes.h"

namespace {

TEST(SisChecksumOracleTest, HistoricalUidAndControllerAndDataCrcsAgree) {
  const char* absl_nullable path = std::getenv("SYMBIAN_SIS_TEST_PACKAGE");
  ASSERT_NE(path, nullptr);
  std::ifstream file(path, std::ios::binary);
  ASSERT_TRUE(file.is_open());
  const std::string bytes{std::istreambuf_iterator<char>(file),
                          std::istreambuf_iterator<char>()};
  using symbian::analysis::internal::Read16;
  using symbian::analysis::internal::Read32;
  using symbian::analysis::internal::Within;
  ASSERT_GE(bytes.size(), 56);
  EXPECT_EQ(Read32(bytes, 0), 0x10201a7a);
  EXPECT_EQ(Read32(bytes, 12),
            (checkSum(bytes.data() + 1) << 16) | checkSum(bytes.data()));
  ASSERT_EQ(Read32(bytes, 24), 34);
  ASSERT_EQ(Read32(bytes, 36), 35);
  ASSERT_EQ(Read32(bytes, 48), 3);
  const size_t controller_length =
      8 + ((static_cast<size_t>(Read32(bytes, 52)) + 3) & ~size_t{3});
  ASSERT_TRUE(Within(bytes.size(), 48, controller_length + 8));
  unsigned short controller_crc = 0;
  Crc(controller_crc, bytes.data() + 48, controller_length);
  EXPECT_EQ(controller_crc, Read16(bytes, 32));
  const size_t data_offset = 48 + controller_length;
  ASSERT_EQ(Read32(bytes, data_offset), 30);
  ASSERT_EQ(bytes.size() - data_offset,
            8 + ((static_cast<size_t>(Read32(bytes, data_offset + 4)) + 3) &
                 ~size_t{3}));
  unsigned short data_crc = 0;
  Crc(data_crc, bytes.data() + data_offset, bytes.size() - data_offset);
  EXPECT_EQ(data_crc, Read16(bytes, 44));
}

}  // namespace
