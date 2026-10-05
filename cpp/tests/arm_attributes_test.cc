#include "symbian/analysis/arm_attributes.h"

#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "e32_fixture.h"
#include "symbian/analysis/bytes.h"
#include "symbian/analysis/elf.h"
#include "symbian/e32/e32.h"

namespace symbian::analysis {
namespace {
using internal::Put16;
using internal::Put32;

std::string Attributes(uint8_t cpu) {
  std::string bytes(24, '\0');
  bytes[0] = 'A';
  Put32(bytes, 1, 23);
  bytes.replace(5, 6, std::string("aeabi\0", 6));
  bytes[11] = 1;
  Put32(bytes, 12, 13);
  bytes[16] = 6;
  bytes[17] = static_cast<char>(cpu);
  bytes[18] = 9;
  bytes[19] = 1;
  bytes[20] = 10;
  bytes[21] = 0;
  bytes[22] = 28;
  bytes[23] = 0;
  return bytes;
}

std::string WithAttributes(std::string elf, std::string_view attributes) {
  const uint32_t attribute_offset = static_cast<uint32_t>(elf.size());
  elf.append(attributes);
  while (elf.size() % 4) {
    elf.push_back('\0');
  }
  const uint32_t table = static_cast<uint32_t>(elf.size());
  // Preserve section identities and their original data locations.
  const uint32_t original_table = internal::Read32(elf, 32);
  const uint16_t count = internal::Read16(elf, 48);
  elf.append(elf.substr(original_table, count * 40));
  elf.append(40, '\0');
  Put32(elf, 32, table);
  Put16(elf, 48, count + 1);
  Put32(elf, table + count * 40 + 4, 0x70000003);
  Put32(elf, table + count * 40 + 16, attribute_offset);
  Put32(elf, table + count * 40 + 20, static_cast<uint32_t>(attributes.size()));
  return elf;
}

TEST(Eka1Test, PublishesLegacyHeaderWithoutEka2EntryOrSecurity) {
  auto elf = WithAttributes(testing::Executable(), Attributes(3));
  // Ordinary ARM return, not the EKA2 marker.
  Put32(elf, 84, 0xe12fff1e);
  const auto image = e32::ConvertEka1Executable(elf, 0xe0000761);
  ASSERT_TRUE(image.ok()) << image.status();
  EXPECT_EQ(image->size(), 124 + 32);
  EXPECT_EQ(internal::Read32(*image, 20), 0x2000);
  EXPECT_EQ(internal::Read32(*image, 44), 0);
  const auto info = e32::InspectImage(*image);
  ASSERT_TRUE(info.ok()) << info.status();
  EXPECT_EQ(info->kernel, "eka1");
  EXPECT_EQ(info->header_size, 124);
  EXPECT_EQ(info->architecture, "armv5t");
  EXPECT_EQ(info->secure_id, 0);
  EXPECT_EQ(info->header_crc, 0);
  EXPECT_TRUE(info->imports.empty());
  for (size_t size = 0; size < image->size(); ++size) {
    EXPECT_FALSE(
        e32::InspectImage(std::string_view(*image).substr(0, size)).ok());
  }
  for (size_t offset : {size_t{8}, size_t{12}, size_t{24}, size_t{48},
                        size_t{72}, size_t{100}, size_t{124}}) {
    auto damaged = *image;
    damaged[offset] ^= 1;
    EXPECT_FALSE(e32::InspectImage(damaged).ok()) << offset;
  }
  EXPECT_FALSE(e32::ConvertPicExecutable(elf, 0xe0000761).ok());
}

TEST(Eka1Test, RejectsUntestedAbiAndRuntimeRequirements) {
  EXPECT_FALSE(
      e32::ConvertEka1Executable(testing::Executable(), 0xe0000761).ok());
  EXPECT_FALSE(
      e32::ConvertEka1Executable(
          WithAttributes(testing::Executable(), Attributes(6)), 0xe0000761)
          .ok());
  auto elf = WithAttributes(testing::Executable(), Attributes(3));
  EXPECT_FALSE(e32::ConvertEka1Executable(elf, 1).ok());
  Put32(elf, 24, 0x8001);  // Thumb entry is not the ARM callable contract.
  EXPECT_FALSE(e32::ConvertEka1Executable(elf, 0xe0000761).ok());
  EXPECT_EQ(
      e32::ConvertEka1Executable(
          WithAttributes(testing::DataExecutable(), Attributes(3)), 0xe0000761)
          .status()
          .code(),
      absl::StatusCode::kUnimplemented);
  auto pointer = WithAttributes(testing::Executable(), Attributes(3));
  Put32(pointer, 100, 0x8018);
  Put32(pointer, 352, 0x102);  // Retained ABS32 requires a code fixup.
  EXPECT_EQ(e32::ConvertEka1Executable(pointer, 0xe0000761).status().code(),
            absl::StatusCode::kUnimplemented);
  auto image = e32::ConvertEka1Executable(
      WithAttributes(testing::Executable(), Attributes(3)), 0xe0000761);
  ASSERT_TRUE(image.ok()) << image.status();
  for (size_t offset : {size_t{44}, size_t{52}, size_t{68}, size_t{84},
                        size_t{88}, size_t{108}, size_t{112}}) {
    auto unsupported = *image;
    Put32(unsupported, offset, 4);
    EXPECT_FALSE(e32::InspectImage(unsupported).ok());
  }
}

TEST(ArmAttributesTest, ReadsBothSupportedIsasAndKeepsMissingMetadataUnknown) {
  for (const uint8_t cpu : {uint8_t{3}, uint8_t{6}}) {
    const auto value = InspectArmAttributes(Attributes(cpu));
    ASSERT_TRUE(value.ok()) << value.status();
    EXPECT_EQ(value->cpu_arch, cpu);
    EXPECT_EQ(value->thumb_isa, 1);
    EXPECT_EQ(value->fp_arch, 0);
    EXPECT_EQ(value->vfp_args, 0);
  }
  EXPECT_EQ(InspectElf32(testing::Executable())->arm.cpu_arch, 0);
}

TEST(ArmAttributesTest, RejectsTruncationsAndMalformedBounds) {
  const auto bytes = Attributes(6);
  for (size_t size = 0; size < bytes.size(); ++size) {
    // A version byte alone carries no vendor block, hence no ISA assertion.
    if (size == 1) {
      continue;
    }
    EXPECT_FALSE(
        InspectArmAttributes(std::string_view(bytes).substr(0, size)).ok())
        << size;
  }
  for (const auto offset : {size_t{1}, size_t{12}}) {
    auto changed = bytes;
    Put32(changed, offset, 0xffffffff);
    EXPECT_FALSE(InspectArmAttributes(changed).ok());
  }
  auto changed = bytes;
  changed[22] = 6;  // Duplicate CPU architecture.
  EXPECT_FALSE(InspectArmAttributes(changed).ok());
  EXPECT_FALSE(InspectArmAttributes(std::string(65537, 'A')).ok());
  changed = bytes;
  changed[17] = static_cast<char>(0x80);
  changed[18] = changed[19] = changed[20] = changed[21] =
      static_cast<char>(0xff);
  EXPECT_FALSE(InspectArmAttributes(changed).ok());
}

TEST(ArmAttributesTest,
     ConvertsMatchedE32CpuIdentityAndRejectsUnsupportedProfiles) {
  for (const uint8_t cpu : {uint8_t{3}, uint8_t{6}}) {
    const auto elf = WithAttributes(testing::Executable(), Attributes(cpu));
    const auto header = InspectElf32(elf);
    ASSERT_TRUE(header.ok()) << header.status();
    EXPECT_EQ(header->arm.cpu_arch, cpu);
    const auto image = e32::ConvertPicExecutable(elf, 0xe0000808);
    ASSERT_TRUE(image.ok()) << image.status();
    const auto info = e32::InspectImage(*image);
    ASSERT_TRUE(info.ok()) << info.status();
    EXPECT_EQ(info->architecture, cpu == 6 ? "armv6" : "armv5t");
  }
  for (const auto& [offset, value] : std::vector<std::pair<size_t, uint8_t>>{
           {17, 10}, {19, 2}, {21, 2}, {23, 1}}) {
    auto attributes = Attributes(6);
    attributes[offset] = static_cast<char>(value);
    const auto image = e32::ConvertPicExecutable(
        WithAttributes(testing::Executable(), attributes), 0xe0000808);
    EXPECT_EQ(image.status().code(), absl::StatusCode::kUnimplemented);
  }
}
}  // namespace
}  // namespace symbian::analysis
