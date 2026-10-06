// SPDX-License-Identifier: GPL-3.0-or-later
// Integration harness for EKA2L1's GPL parser. Not part of the Python wheel.

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

#include <absl/base/nullability.h>
#include <common/buffer.h>
#include <gtest/gtest.h>
#include <loader/e32img.h>

namespace {

class E32OracleTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const char* absl_nullable path = std::getenv("SYMBIAN_E32_TEST_IMAGE");
    ASSERT_NE(path, nullptr) << "SYMBIAN_E32_TEST_IMAGE must name a real build";
    std::ifstream stream(path, std::ios::binary);
    ASSERT_TRUE(stream.is_open()) << path;
    bytes_.assign(std::istreambuf_iterator<char>(stream),
                  std::istreambuf_iterator<char>());
    ASSERT_GE(bytes_.size(), 172);
  }

  std::optional<eka2l1::loader::e32img> Parse() {
    eka2l1::common::ro_buf_stream stream(
        reinterpret_cast<uint8_t*>(bytes_.data()), bytes_.size());
    return eka2l1::loader::parse_e32img(&stream, true);
  }

  std::string bytes_;
};

TEST_F(E32OracleTest, AcceptsNativeConverterOutputAndMetadata) {
  const auto image = Parse();
  ASSERT_TRUE(image.has_value());
  EXPECT_EQ(image->header.uid1, eka2l1::loader::e32_img_type::exe);
  EXPECT_EQ(image->header.uid3, 0xe0000808);
  EXPECT_EQ(image->header.code_offset, 156);
  EXPECT_LE(image->header.code_size, bytes_.size() - 156);
  EXPECT_EQ(image->header.code_reloc_offset,
            image->header.code_offset + image->header.code_size);
  EXPECT_EQ(image->header.entry_point, 0);
  EXPECT_EQ(image->header.flags, 0x12000028);
  EXPECT_EQ(image->header.cpu, eka2l1::loader::e32_cpu::armv5);
  EXPECT_EQ(image->header_extended.info.secure_id, 0xe0000808);
  EXPECT_EQ(image->header_extended.info.cap1, 0);
  EXPECT_EQ(image->header_extended.info.cap2, 0);
  EXPECT_TRUE(image->import_section.imports.empty());
  EXPECT_EQ(image->code_reloc_section.num_relocs, 4);
  const uint32_t descriptor = image->header_extended.exception_des & ~1U;
  ASSERT_NE(descriptor, 0);
  std::vector<uint32_t> offsets;
  for (const auto& page : image->code_reloc_section.entries) {
    for (const uint16_t word : page.rels_info) {
      if (word == 0) {
        continue;
      }
      EXPECT_EQ(word & 0xf000, 0x1000);
      offsets.push_back(page.base + (word & 0xfff));
    }
  }
  EXPECT_EQ(offsets, (std::vector<uint32_t>{descriptor, descriptor + 4,
                                            descriptor + 8, descriptor + 12}));
}

TEST_F(E32OracleTest, RejectsDamagedUidChecksum) {
  bytes_[12] ^= 1;
  EXPECT_FALSE(Parse().has_value());
}

TEST_F(E32OracleTest, RejectsTruncatedCode) {
  const auto image = Parse();
  ASSERT_TRUE(image.has_value());
  bytes_.resize(image->header.code_offset + image->header.code_size - 1);
  EXPECT_FALSE(Parse().has_value());
}

TEST_F(E32OracleTest, DocumentsMissingHeaderCrcVerification) {
  // Parser acceptance is weaker than the real Symbian loader's validation.
  // This deliberately records a blind spot; our native parser rejects it.
  bytes_[20] ^= 1;
  EXPECT_TRUE(Parse().has_value());
}

}  // namespace
