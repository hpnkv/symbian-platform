#include "symbian/e32/e32.h"

#include <cstddef>
#include <string>

#include <absl/status/status.h>
#include <gtest/gtest.h>

#include "e32_fixture.h"
#include "symbian/analysis/bytes.h"
#include "symbian/e32/exports.h"
#include "symbian/e32/imports.h"

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

TEST(E32Test, RejectsOutOfRangeAbsoluteAndExternalRelocations) {
  std::string elf = Executable();
  Put32(elf, 352, 0x102);  // R_ARM_ABS32.
  EXPECT_EQ(ConvertPicExecutable(elf, 0xe0000808).status().code(),
            absl::StatusCode::kDataLoss);
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

TEST(E32Test, PreservesDllBlocksAndSparseOrdinals) {
  const std::vector<ImportBlock> blocks{{"a.dll", {{16, 7}, {24, 641}}},
                                        {"b.dll", {{20, 42}}}};
  std::string code(32, '\0');
  Put32(code, 16, 7);
  Put32(code, 24, 641);
  Put32(code, 20, 42);
  const auto section = internal::EncodeImports(blocks);
  EXPECT_EQ(Read32(section, 0), section.size());
  const auto parsed = internal::DecodeImports(section, code, 2);
  ASSERT_TRUE(parsed.ok()) << parsed.status();
  EXPECT_EQ(parsed->at(0).dll, "a.dll");
  EXPECT_EQ(parsed->at(0).slots.at(1).ordinal, 641);
  EXPECT_EQ(parsed->at(1).slots.at(0).code_offset, 20);
}

TEST(E32Test, RejectsImportSectionTruncationsAndBadPointers) {
  std::string code(32, '\0');
  Put32(code, 16, 641);
  const auto section = internal::EncodeImports({{"euser.dll", {{16, 641}}}});
  for (size_t i = 0; i < section.size(); ++i) {
    EXPECT_FALSE(
        internal::DecodeImports(std::string_view(section).substr(0, i), code, 1)
            .ok());
  }
  for (size_t p : {size_t{0}, size_t{4}, size_t{8}, size_t{12}}) {
    std::string changed = section;
    Put32(changed, p, 0xfffffff0);
    EXPECT_FALSE(internal::DecodeImports(changed, code, 1).ok());
  }
  EXPECT_FALSE(internal::DecodeImports(section + '\0', code, 1).ok());
  EXPECT_FALSE(internal::DecodeImports(section, code, 0).ok());
  EXPECT_FALSE(internal::DecodeImports(section, code, 17).ok());
}

TEST(E32Test, RejectsDuplicateSlotsAndUnsupportedImportContracts) {
  std::string code(32, '\0');
  Put32(code, 16, 7);
  for (const std::vector<ImportBlock>& blocks :
       {std::vector<ImportBlock>{{"a.dll", {{16, 7}, {16, 7}}}},
        std::vector<ImportBlock>{{"a.dll", {{16, 7}}}, {"b.dll", {{16, 7}}}},
        std::vector<ImportBlock>{{"../a.dll", {{16, 7}}}},
        std::vector<ImportBlock>{{"a.dll", {{12, 7}}}},
        std::vector<ImportBlock>{{"a.dll", {}}}}) {
    EXPECT_FALSE(internal::DecodeImports(internal::EncodeImports(blocks), code,
                                         static_cast<uint32_t>(blocks.size()))
                     .ok());
  }
  const auto section = internal::EncodeImports({{"a.dll", {{16, 7}}}});
  Put32(code, 16, 0);
  EXPECT_FALSE(internal::DecodeImports(section, code, 1).ok());
  Put32(code, 16, 0x10007);
  EXPECT_EQ(internal::DecodeImports(section, code, 1).status().code(),
            absl::StatusCode::kUnimplemented);
}

TEST(E32Test, ImportCallMustReachItsOwnPcRelativeVeneerSlot) {
  std::string elf(128, '\0');
  const internal::Segment code{0, 0x8000, 128};
  Put32(elf, 0, 0xeb000006);  // ARM BL to code + 32.
  Put32(elf, 32, 0xe28fc000);
  Put32(elf, 36, 0xe28cc000);
  Put32(elf, 40, 0xe5bcf018);  // Slot at code + 64.
  EXPECT_TRUE(internal::CheckImportCall(elf, code, 0x8000, 28, 64).ok());
  EXPECT_FALSE(internal::CheckImportCall(elf, code, 0x8000, 28, 68).ok());
  Put32(elf, 32, 0xe59fc000);  // Absolute veneer cannot pass.
  EXPECT_FALSE(internal::CheckImportCall(elf, code, 0x8000, 28, 64).ok());
}

TEST(E32PointerTest, EmitsRuntimeFixupWithoutReapplyingLinkedSymbol) {
  std::string elf = Executable();
  Put32(elf, 100, 0x8019);
  Put32(elf, 336, 0x8019);
  elf[344] = 0x12;
  Put32(elf, 352, 0x102);
  const auto image = ConvertPicExecutable(elf, 0xe0000808);
  ASSERT_TRUE(image.ok()) << image.status();
  const auto info = InspectImage(*image);
  ASSERT_TRUE(info.ok()) << info.status();
  EXPECT_EQ(info->code_relocations, std::vector<uint32_t>{16});
  EXPECT_EQ(Read32(*image, 156 + 16), 0x8019);  // Preserve Thumb bit.
  EXPECT_FALSE(info->dll);
  std::string changed = *image;
  Put32(changed, 156 + 16, 0xfffffffc);
  EXPECT_FALSE(InspectImage(changed).ok());
}

TEST(E32PointerTest, RejectsBadPointerStateAlignmentAndDuplicateFixups) {
  std::string elf = Executable();
  Put32(elf, 100, 0x8018);
  Put32(elf, 336, 0x8019);
  elf[344] = 0x12;
  Put32(elf, 352, 0x102);
  EXPECT_EQ(ConvertPicExecutable(elf, 0xe0000808).status().code(),
            absl::StatusCode::kDataLoss);
  Put32(elf, 100, 0x8019);
  Put32(elf, 348, 0x8011);
  EXPECT_EQ(ConvertPicExecutable(elf, 0xe0000808).status().code(),
            absl::StatusCode::kUnimplemented);
  Put32(elf, 348, 0x8010);
  elf.append(8, '\0');
  Put32(elf, 356, 0x8010);
  Put32(elf, 360, 0x102);
  Put32(elf, 256, 16);
  EXPECT_EQ(ConvertPicExecutable(elf, 0xe0000808).status().code(),
            absl::StatusCode::kDataLoss);
}

// Extend the bounded format fixture with a real named function symbol.
std::string ExportedExecutable() {
  std::string elf = Executable();
  Put32(elf, 292, static_cast<uint32_t>(elf.size()));
  Put32(elf, 296, 10);
  elf.append("\0Function\0", 10);
  Put32(elf, 332, 1);
  Put32(elf, 340, 4);
  elf[344] = 0x12;  // STB_GLOBAL, STT_FUNC.
  return elf;
}

TEST(E32DllTest, ResolvesFrozenOrdinalsAndRelocatesAbsentSlots) {
  const auto image = ConvertDll(
      ExportedExecutable(),
      "EXPORTS\nFunction @ 7 NONAME\nGone @ 3 NONAME ABSENT\n", {}, 0xe0000810);
  ASSERT_TRUE(image.ok()) << image.status();
  const auto info = InspectImage(*image);
  ASSERT_TRUE(info.ok()) << info.status();
  EXPECT_TRUE(info->dll);
  EXPECT_EQ(info->exports.size(), 7);
  EXPECT_EQ(info->exports[6].address, 0x8018);
  EXPECT_FALSE(info->exports[6].absent);
  EXPECT_EQ(info->exports[2].address, 0x8000);
  EXPECT_TRUE(info->exports[2].absent);
  EXPECT_EQ(info->code_relocations.size(), 7);
  EXPECT_EQ(Read32(*image, Read32(*image, 88) - 4), 7);
  EXPECT_EQ(static_cast<uint8_t>((*image)[155]), 0xc0);
}

TEST(E32DllTest, KeepsApplicationPointerAndExportRelocationsTogether) {
  std::string elf = ExportedExecutable();
  Put32(elf, 100, 0x8018);
  Put32(elf, 352, 0x102);
  const auto image =
      ConvertDll(elf, "EXPORTS\nFunction @ 7 NONAME\n", {}, 0xe0000810);
  ASSERT_TRUE(image.ok()) << image.status();
  const auto info = InspectImage(*image);
  ASSERT_TRUE(info.ok()) << info.status();
  EXPECT_EQ(info->code_relocations.size(), 8);
  EXPECT_EQ(info->code_relocations[0], 16);
  EXPECT_EQ(info->code_relocations[1], 36);
  EXPECT_EQ(Read32(*image, 156 + 16), 0x8018);
}

TEST(E32DllTest, PreservesThumbBitAndSupportsLargeVariableHeader) {
  std::string elf = ExportedExecutable();
  Put32(elf, 336, 0x8019);
  const auto image =
      ConvertDll(elf, "EXPORTS\nFunction @ 641 NONAME\n", {}, 0xe0000810);
  ASSERT_TRUE(image.ok()) << image.status();
  const auto info = InspectImage(*image);
  ASSERT_TRUE(info.ok()) << info.status();
  EXPECT_EQ(info->header_size, 236);
  EXPECT_EQ(info->exports.back().address, 0x8019);
  EXPECT_EQ(info->exports.size(), 641);
  EXPECT_EQ(info->code_relocations.size(), 641);
}

TEST(E32DllTest, SupportsNoHolesAndMaximumFrozenOrdinal) {
  for (const uint32_t count : {1U, 65535U}) {
    const auto image =
        ConvertDll(ExportedExecutable(),
                   "EXPORTS\nFunction @ " + std::to_string(count) + " NONAME\n",
                   {}, 0xe0000810);
    ASSERT_TRUE(image.ok()) << image.status();
    const auto info = InspectImage(*image);
    ASSERT_TRUE(info.ok()) << info.status();
    EXPECT_EQ(info->exports.size(), count);
    EXPECT_EQ(info->header_size, count == 1 ? 156 : 8348);
    EXPECT_FALSE(info->exports.back().absent);
  }
}

TEST(E32DllTest, RejectsMissingDataAndInvalidExportDefinitions) {
  EXPECT_EQ(ConvertDll(ExportedExecutable(), "EXPORTS\nMissing @ 1 NONAME\n",
                       {}, 0xe0000810)
                .status()
                .code(),
            absl::StatusCode::kNotFound);
  EXPECT_EQ(ConvertDll(ExportedExecutable(),
                       "EXPORTS\nFunction @ 1 NONAME DATA 4\n", {}, 0xe0000810)
                .status()
                .code(),
            absl::StatusCode::kUnimplemented);
  std::string elf = ExportedExecutable();
  elf[344] = 0x11;
  EXPECT_EQ(ConvertDll(elf, "EXPORTS\nFunction @ 1 NONAME\n", {}, 0xe0000810)
                .status()
                .code(),
            absl::StatusCode::kUnimplemented);
}

TEST(E32DllTest, RejectsTruncationsAndExportRelocationCorruption) {
  const auto image = ConvertDll(
      ExportedExecutable(), "EXPORTS\nFunction @ 7 NONAME\n", {}, 0xe0000810);
  ASSERT_TRUE(image.ok()) << image.status();
  for (size_t i = 0; i < image->size(); ++i) {
    EXPECT_FALSE(InspectImage(std::string_view(*image).substr(0, i)).ok()) << i;
  }
  const size_t directory = Read32(*image, 88);
  const size_t relocations = Read32(*image, 112);
  for (size_t p : {directory - 4, directory, directory + 24, relocations,
                   relocations + 4, relocations + 12}) {
    std::string bytes = *image;
    Put32(bytes, p, 0xffffffff);
    EXPECT_FALSE(InspectImage(bytes).ok()) << p;
  }
}

TEST(E32DllTest, BoundsRelocationPagesCountsAndCanonicalPadding) {
  const std::vector<uint32_t> offsets{4, 4092, 4096, 4100, 8192};
  const auto encoded = internal::EncodeCodeRelocations(offsets);
  ASSERT_TRUE(encoded.ok()) << encoded.status();
  const auto decoded = internal::DecodeCodeRelocations(*encoded, 8196);
  ASSERT_TRUE(decoded.ok()) << decoded.status();
  EXPECT_EQ(*decoded, offsets);
  EXPECT_FALSE(internal::DecodeCodeRelocations(*encoded, 8195).ok());
  EXPECT_FALSE(internal::EncodeCodeRelocations({4, 4}).ok());
  EXPECT_FALSE(internal::EncodeCodeRelocations({4096, 4}).ok());
  EXPECT_FALSE(internal::EncodeCodeRelocations({2}).ok());
  std::string changed = *encoded;
  changed.back() = 1;
  EXPECT_FALSE(internal::DecodeCodeRelocations(changed, 8196).ok());
}

}  // namespace
}  // namespace symbian::e32
