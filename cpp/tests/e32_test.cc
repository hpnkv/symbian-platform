#include "symbian/e32/e32.h"

#include <cstddef>
#include <string>

#include <absl/status/status.h>
#include <gtest/gtest.h>

#include "e32_fixture.h"
#include "symbian/analysis/bytes.h"

namespace symbian::e32 {
namespace {

using analysis::internal::Put16;
using analysis::internal::Put32;
using analysis::internal::Read32;

using testing::Executable;

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
