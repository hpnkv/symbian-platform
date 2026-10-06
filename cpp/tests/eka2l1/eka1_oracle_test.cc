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

TEST(Eka1OracleTest, IndependentlyChecksLegacyHeaderAndBounds) {
  const char* absl_nullable path = std::getenv("SYMBIAN_EKA1_TEST_IMAGE");
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
  const char* absl_nullable path =
      std::getenv("SYMBIAN_EKA1_IMPORT_TEST_IMAGE");
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

}  // namespace
