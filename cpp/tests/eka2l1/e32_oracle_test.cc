// SPDX-License-Identifier: GPL-3.0-or-later
// Integration harness for EKA2L1's GPL parser. Not part of the Python wheel.

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

#include <common/buffer.h>
#include <gtest/gtest.h>
#include <loader/e32img.h>

namespace {

class E32OracleTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const char* path = std::getenv("SYMBIAN_E32_TEST_IMAGE");
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

TEST(Eka1OracleTest, IndependentlyChecksLegacyHeaderAndBounds) {
  const char* path = std::getenv("SYMBIAN_EKA1_TEST_IMAGE");
  if (path == nullptr) {
    GTEST_SKIP() << "Set SYMBIAN_EKA1_TEST_IMAGE";
  }
  std::ifstream file(path, std::ios::binary);
  ASSERT_TRUE(file.is_open());
  std::string bytes{std::istreambuf_iterator<char>(file),
                    std::istreambuf_iterator<char>()};
  auto parse = [&]() {
    eka2l1::common::ro_buf_stream stream(
        reinterpret_cast<uint8_t*>(bytes.data()), bytes.size());
    return eka2l1::loader::parse_e32img(&stream, true);
  };
  const auto image = parse();
  ASSERT_TRUE(image.has_value());
  EXPECT_EQ(image->epoc_ver, epocver::epoc6);
  EXPECT_EQ(image->header.uid1, eka2l1::loader::e32_img_type::exe);
  EXPECT_EQ(image->header.uid3, 0xe0000761);
  EXPECT_EQ(image->header.code_offset, 124);
  EXPECT_EQ(image->header.code_size, bytes.size() - 124);
  EXPECT_EQ(image->header.text_size, image->header.code_size);
  EXPECT_EQ(image->header.entry_point, 0);
  EXPECT_EQ(image->header.flags, 0);
  EXPECT_EQ(static_cast<uint32_t>(image->header.cpu), 0x2000);
  EXPECT_EQ(image->header.data_size, 0);
  EXPECT_EQ(image->header.bss_size, 0);
  EXPECT_TRUE(image->import_section.imports.empty());
  EXPECT_TRUE(image->code_reloc_section.entries.empty());
  EXPECT_FALSE(image->has_extended_header);
  bytes[12] ^= 1;
  EXPECT_FALSE(parse().has_value());
  bytes[12] ^= 1;
  bytes.pop_back();
  EXPECT_FALSE(parse().has_value());
}

TEST(Eka1OracleTest, IndependentlyChecksPeImportsAndIat) {
  const char* path = std::getenv("SYMBIAN_EKA1_IMPORT_TEST_IMAGE");
  if (path == nullptr) {
    GTEST_SKIP() << "Set SYMBIAN_EKA1_IMPORT_TEST_IMAGE";
  }
  std::ifstream file(path, std::ios::binary);
  ASSERT_TRUE(file.is_open());
  std::string bytes{std::istreambuf_iterator<char>(file),
                    std::istreambuf_iterator<char>()};
  auto parse = [&]() {
    eka2l1::common::ro_buf_stream stream(
        reinterpret_cast<uint8_t*>(bytes.data()), bytes.size());
    return eka2l1::loader::parse_e32img(&stream, true);
  };
  const auto image = parse();
  ASSERT_TRUE(image.has_value());
  EXPECT_EQ(image->epoc_ver, epocver::epoc6);
  EXPECT_EQ(image->header.uid1, eka2l1::loader::e32_img_type::exe);
  EXPECT_EQ(image->header.uid3, 0xe0000761);
  EXPECT_EQ(image->header.code_offset, 124);
  EXPECT_EQ(image->header.flags, 0);
  EXPECT_EQ(static_cast<uint32_t>(image->header.cpu), 0x2000);
  EXPECT_EQ(image->header.entry_point, 0);
  EXPECT_EQ(image->header.data_size, 0);
  EXPECT_EQ(image->header.bss_size, 0);
  EXPECT_EQ(image->header.dll_ref_table_count, 1);
  EXPECT_EQ(image->header.import_offset, 124 + image->header.code_size);
  EXPECT_EQ(image->header.code_size - image->header.text_size, 24);
  ASSERT_EQ(image->import_section.imports.size(), 1);
  const auto& block = image->import_section.imports.front();
  EXPECT_EQ(block.dll_name, "euser.dll");
  EXPECT_EQ(block.number_of_imports, 5);
  const std::vector<uint32_t> ordinals{41, 45, 39, 243, 476};
  EXPECT_EQ(block.ordinals, ordinals);  // PE ordinals, not ELF code offsets.
  EXPECT_EQ(image->iat.its, ordinals);
  EXPECT_TRUE(image->code_reloc_section.entries.empty());
  EXPECT_TRUE(image->data_reloc_section.entries.empty());
  EXPECT_FALSE(image->has_extended_header);
  bytes[12] ^= 1;
  EXPECT_FALSE(parse().has_value());
  bytes[12] ^= 1;
  bytes.resize(image->header.import_offset - 1);
  EXPECT_FALSE(parse().has_value());
}

TEST_F(E32OracleTest, AcceptsNativeConverterOutputAndMetadata) {
  const auto image = Parse();
  ASSERT_TRUE(image.has_value());
  EXPECT_EQ(image->header.uid1, eka2l1::loader::e32_img_type::exe);
  EXPECT_EQ(image->header.uid3, 0xe0000808);
  EXPECT_EQ(image->header.code_offset, 156);
  EXPECT_EQ(image->header.code_size, bytes_.size() - 156);
  EXPECT_EQ(image->header.entry_point, 0);
  EXPECT_EQ(image->header.flags, 0x12000028);
  EXPECT_EQ(image->header.cpu, eka2l1::loader::e32_cpu::armv5);
  EXPECT_EQ(image->header_extended.info.secure_id, 0xe0000808);
  EXPECT_EQ(image->header_extended.info.cap1, 0);
  EXPECT_EQ(image->header_extended.info.cap2, 0);
  EXPECT_TRUE(image->import_section.imports.empty());
  EXPECT_TRUE(image->code_reloc_section.entries.empty());
}

TEST_F(E32OracleTest, RejectsDamagedUidChecksum) {
  bytes_[12] ^= 1;
  EXPECT_FALSE(Parse().has_value());
}

TEST_F(E32OracleTest, RejectsTruncatedCode) {
  bytes_.pop_back();
  EXPECT_FALSE(Parse().has_value());
}

TEST_F(E32OracleTest, DocumentsMissingHeaderCrcVerification) {
  // Parser acceptance is weaker than the real Symbian loader's validation.
  // This deliberately records a blind spot; our native parser rejects it.
  bytes_[20] ^= 1;
  EXPECT_TRUE(Parse().has_value());
}

}  // namespace
