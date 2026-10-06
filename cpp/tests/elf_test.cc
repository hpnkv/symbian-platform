#include "symbian/analysis/elf.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

#include <absl/base/nullability.h>
#include <absl/status/status.h>
#include <gtest/gtest.h>

namespace symbian::analysis {
namespace {

void Put16(std::span<char> bytes, size_t offset, uint16_t value) {
  bytes[offset] = static_cast<char>(value & 0xff);
  bytes[offset + 1] = static_cast<char>(value >> 8);
}

void Put32(std::span<char> bytes, size_t offset, uint32_t value) {
  for (size_t i = 0; i < 4; ++i) {
    bytes[offset + i] = static_cast<char>((value >> (i * 8)) & 0xff);
  }
}

std::string Header() {
  std::string bytes(52, '\0');
  bytes.replace(0, 4,
                "\x7f"
                "ELF");
  bytes[4] = bytes[5] = bytes[6] = 1;
  Put16(bytes, 16, 1);
  Put16(bytes, 18, 40);
  Put32(bytes, 20, 1);
  Put32(bytes, 36, 0x05000000);
  Put16(bytes, 40, 52);
  return bytes;
}

TEST(ElfTest, ReadsArmMetadata) {
  const auto result = InspectElf32(Header());
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_EQ(result->type, 1);
  EXPECT_EQ(result->machine, 40);
  EXPECT_EQ(result->flags, 0x05000000);
}

TEST(ElfTest, RejectsEveryHeaderTruncation) {
  const std::string bytes = Header();
  for (size_t size = 0; size < bytes.size(); ++size) {
    EXPECT_FALSE(InspectElf32(std::string_view(bytes).substr(0, size)).ok());
  }
}

TEST(ElfTest, DistinguishesUnsupportedFormat) {
  std::string bytes = Header();
  bytes[4] = 2;
  EXPECT_EQ(InspectElf32(bytes).status().code(),
            absl::StatusCode::kUnimplemented);
  bytes[4] = 1;
  bytes[5] = 2;
  EXPECT_EQ(InspectElf32(bytes).status().code(),
            absl::StatusCode::kUnimplemented);
}

TEST(ElfTest, RejectsInvalidMagicVersionAndSize) {
  for (size_t offset : {size_t{0}, size_t{6}, size_t{20}, size_t{40}}) {
    std::string bytes = Header();
    bytes[offset] = 0;
    EXPECT_EQ(InspectElf32(bytes).status().code(), absl::StatusCode::kDataLoss);
  }
}

TEST(ElfTest, BoundsSectionTableWithoutIntegerOverflow) {
  std::string bytes = Header();
  Put32(bytes, 32, 0xfffffff0);
  Put16(bytes, 46, 40);
  Put16(bytes, 48, 0xfffe);
  EXPECT_EQ(InspectElf32(bytes).status().code(), absl::StatusCode::kDataLoss);
}

TEST(ElfTest, BoundsProgramTable) {
  std::string bytes = Header();
  Put32(bytes, 28, 52);
  Put16(bytes, 42, 32);
  Put16(bytes, 44, 1);
  EXPECT_FALSE(InspectElf32(bytes).ok());
  bytes.resize(84, '\0');
  EXPECT_TRUE(InspectElf32(bytes).ok());
}

TEST(ElfTest, RejectsUndersizedAndOverlappingTables) {
  std::string bytes = Header();
  bytes.resize(100, '\0');
  Put32(bytes, 32, 52);
  Put16(bytes, 46, 39);
  Put16(bytes, 48, 1);
  EXPECT_FALSE(InspectElf32(bytes).ok());
  Put16(bytes, 46, 40);
  Put32(bytes, 32, 16);
  EXPECT_FALSE(InspectElf32(bytes).ok());
}

TEST(ElfTest, RejectsInvalidStringTableAndExtendedNumbering) {
  std::string bytes = Header();
  Put16(bytes, 50, 1);
  EXPECT_EQ(InspectElf32(bytes).status().code(), absl::StatusCode::kDataLoss);
  Put16(bytes, 50, 0xffff);
  EXPECT_EQ(InspectElf32(bytes).status().code(),
            absl::StatusCode::kUnimplemented);
}

}  // namespace
}  // namespace symbian::analysis
