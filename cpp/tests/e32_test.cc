#include "symbian/e32/e32.h"

#include <cstddef>
#include <string>

#include <absl/status/status.h>
#include <gtest/gtest.h>

#include "symbian/analysis/bytes.h"

namespace symbian::e32 {
namespace {

using analysis::internal::Put16;
using analysis::internal::Put32;
using analysis::internal::Read32;

// A complete, bounded ET_EXEC with one RX segment, a startup marker and a
// retained R_ARM_CALL to an internal symbol. This is a format fixture only.
std::string Executable() {
  std::string bytes(356, '\0');
  bytes.replace(0, 4,
                "\x7f"
                "ELF");
  bytes[4] = bytes[5] = bytes[6] = 1;
  Put16(bytes, 16, 2);
  Put16(bytes, 18, 40);
  Put32(bytes, 20, 1);
  Put32(bytes, 24, 0x8000);
  Put32(bytes, 28, 52);
  Put32(bytes, 32, 116);
  Put32(bytes, 36, 0x05000200);
  Put16(bytes, 40, 52);
  Put16(bytes, 42, 32);
  Put16(bytes, 44, 1);
  Put16(bytes, 46, 40);
  Put16(bytes, 48, 5);
  Put32(bytes, 52, 1);
  Put32(bytes, 56, 84);
  Put32(bytes, 60, 0x8000);
  Put32(bytes, 68, 32);
  Put32(bytes, 72, 32);
  Put32(bytes, 76, 5);
  Put32(bytes, 80, 4);
  Put32(bytes, 84, 0xe31f0000);
  Put32(bytes, 88, 0xe3540001);
  Put32(bytes, 92, 0xea000000);
  Put32(bytes, 100, 0xeb000000);
  // Section 1: text. Section 2: symbols. Section 3: REL. Section 4: strings.
  Put32(bytes, 160, 1);
  Put32(bytes, 164, 6);
  Put32(bytes, 168, 0x8000);
  Put32(bytes, 172, 84);
  Put32(bytes, 176, 32);
  Put32(bytes, 200, 2);
  Put32(bytes, 212, 316);
  Put32(bytes, 216, 32);
  Put32(bytes, 220, 4);
  Put32(bytes, 232, 16);
  Put32(bytes, 240, 9);
  Put32(bytes, 252, 348);
  Put32(bytes, 256, 8);
  Put32(bytes, 260, 2);
  Put32(bytes, 264, 1);
  Put32(bytes, 272, 8);
  Put32(bytes, 280, 3);
  Put32(bytes, 292, 0);
  Put32(bytes, 336, 0x8018);
  Put16(bytes, 346, 1);
  Put32(bytes, 348, 0x8010);
  Put32(bytes, 352, 0x11c);
  return bytes;
}

TEST(E32Test, ConvertsSupportedExecutableWithStableIdentity) {
  const auto result = ConvertPicExecutable(Executable(), 0xe0000808);
  ASSERT_TRUE(result.ok()) << result.status();
  ASSERT_EQ(result->size(), 188);
  EXPECT_EQ(result->substr(16, 4), "EPOC");
  EXPECT_EQ(Read32(*result, 0), 0x1000007a);
  EXPECT_EQ(Read32(*result, 44), 0x12000028);
  EXPECT_EQ(result->substr(156), Executable().substr(84, 32));
  const auto info = InspectImage(*result);
  ASSERT_TRUE(info.ok()) << info.status();
  EXPECT_EQ(info->uid3, 0xe0000808);
  EXPECT_EQ(info->secure_id, info->uid3);
  EXPECT_EQ(info->code_size, 32);
  EXPECT_EQ(info->entry_offset, 0);
  EXPECT_EQ(*result, *ConvertPicExecutable(Executable(), 0xe0000808));
}

TEST(E32Test, RejectsEveryElfTruncation) {
  const std::string elf = Executable();
  for (size_t i = 0; i < elf.size(); ++i) {
    EXPECT_FALSE(
        ConvertPicExecutable(std::string_view(elf).substr(0, i), 0xe0000808)
            .ok())
        << i;
  }
}

TEST(E32Test, RejectsUnsupportedTargetAndUid) {
  EXPECT_EQ(ConvertPicExecutable(Executable(), 0x20000808).status().code(),
            absl::StatusCode::kInvalidArgument);
  std::string elf = Executable();
  Put16(elf, 18, 62);
  EXPECT_EQ(ConvertPicExecutable(elf, 0xe0000808).status().code(),
            absl::StatusCode::kUnimplemented);
}

TEST(E32Test, RejectsAbsoluteAndExternalRelocations) {
  std::string elf = Executable();
  Put32(elf, 352, 0x102);  // R_ARM_ABS32.
  EXPECT_EQ(ConvertPicExecutable(elf, 0xe0000808).status().code(),
            absl::StatusCode::kUnimplemented);
  Put32(elf, 352, 0x11c);
  Put16(elf, 346, 0);  // Undefined symbol.
  EXPECT_EQ(ConvertPicExecutable(elf, 0xe0000808).status().code(),
            absl::StatusCode::kUnimplemented);
}

TEST(E32Test, RequiresRetainedRelocationsAndEntryContract) {
  std::string elf = Executable();
  Put32(elf, 256, 0);
  EXPECT_EQ(ConvertPicExecutable(elf, 0xe0000808).status().code(),
            absl::StatusCode::kFailedPrecondition);
  for (size_t offset : {size_t{84}, size_t{96}}) {
    elf = Executable();
    Put32(elf, offset, 1);
    EXPECT_EQ(ConvertPicExecutable(elf, 0xe0000808).status().code(),
              absl::StatusCode::kFailedPrecondition);
  }
}

TEST(E32Test, BoundsSectionsSymbolsAndRelocations) {
  for (size_t offset : {size_t{56}, size_t{172}, size_t{212}, size_t{252},
                        size_t{260}, size_t{264}, size_t{348}, size_t{352}}) {
    std::string elf = Executable();
    Put32(elf, offset, 0xfffffff0);
    EXPECT_FALSE(ConvertPicExecutable(elf, 0xe0000808).ok()) << offset;
  }
}

TEST(E32Test, RejectsWritableMemoryAndAllocatedData) {
  std::string elf = Executable();
  Put32(elf, 76, 7);
  EXPECT_EQ(ConvertPicExecutable(elf, 0xe0000808).status().code(),
            absl::StatusCode::kUnimplemented);
  elf = Executable();
  Put32(elf, 164, 7);
  EXPECT_EQ(ConvertPicExecutable(elf, 0xe0000808).status().code(),
            absl::StatusCode::kUnimplemented);
}

TEST(E32Test, DetectsHeaderTamperingAndTruncatedCode) {
  const auto image = ConvertPicExecutable(Executable(), 0xe0000808);
  ASSERT_TRUE(image.ok());
  for (size_t i = 0; i < image->size(); ++i) {
    EXPECT_FALSE(InspectImage(std::string_view(*image).substr(0, i)).ok());
  }
  for (size_t offset : {size_t{8}, size_t{12}, size_t{20}, size_t{48},
                        size_t{56}, size_t{128}}) {
    std::string bytes = *image;
    bytes[offset] ^= 1;
    EXPECT_EQ(InspectImage(bytes).status().code(), absl::StatusCode::kDataLoss);
  }
}

}  // namespace
}  // namespace symbian::e32
