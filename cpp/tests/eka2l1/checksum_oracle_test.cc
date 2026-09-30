// Independent oracle using Nokia's unmodified EPL-1.0 checksum.cpp.

#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>

#include <gtest/gtest.h>

#include "checksum.h"
#include "symbian/analysis/bytes.h"

namespace {

TEST(ChecksumOracleTest, MatchesHistoricalUidAndHeaderChecksums) {
  const char* path = std::getenv("SYMBIAN_E32_TEST_IMAGE");
  ASSERT_NE(path, nullptr);
  std::ifstream stream(path, std::ios::binary);
  ASSERT_TRUE(stream.is_open()) << path;
  std::string bytes{std::istreambuf_iterator<char>(stream),
                    std::istreambuf_iterator<char>()};
  ASSERT_GE(bytes.size(), 156);
  using symbian::analysis::internal::Put32;
  using symbian::analysis::internal::Read32;
  const unsigned long uid_crc =
      (checkSum(bytes.data() + 1) << 16) | checkSum(bytes.data());
  EXPECT_EQ(uid_crc, Read32(bytes, 12));
  const uint32_t supplied_crc = Read32(bytes, 20);
  Put32(bytes, 20, 0xc90fdaa2);
  unsigned long header_crc = 0;
  const uint32_t header_size = Read32(bytes, 100);
  ASSERT_GE(header_size, 156);
  ASSERT_LE(header_size, 8348);
  ASSERT_LE(header_size, bytes.size());
  Crc32(header_crc, bytes.data(), header_size);
  EXPECT_EQ(header_crc, supplied_crc);
}

}  // namespace
