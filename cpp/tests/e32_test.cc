#include "symbian/e32/e32.h"

#include <cstddef>
#include <set>
#include <string>
#include <utility>
#include <vector>

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
  EXPECT_EQ(info->exception_descriptor_offset, 0);
  EXPECT_EQ(*result, *ConvertPicExecutable(Executable(), 0xe0000808));
}

TEST(E32Test, OptsInToNetworkServicesCapability) {
  constexpr uint32_t kNetworkServices = 1U << 13;
  const auto image =
      ConvertPicExecutable(Executable(), 0xe0000808, kNetworkServices);
  ASSERT_TRUE(image.ok()) << image.status();
  EXPECT_EQ(Read32(*image, 136), kNetworkServices);
  const auto inspected = InspectImage(*image);
  ASSERT_TRUE(inspected.ok()) << inspected.status();
  EXPECT_EQ(inspected->capabilities, kNetworkServices);
  EXPECT_EQ(
      ConvertPicExecutable(Executable(), 0xe0000808, 1U << 10).status().code(),
      absl::StatusCode::kUnimplemented);
}

TEST(E32Test, RejectsRenamedArmExceptionIndexWithoutDescriptor) {
  auto elf = Executable();
  Put32(elf, 160, 0x70000001);  // SHT_ARM_EXIDX without .ARM.exidx name.
  EXPECT_EQ(ConvertPicExecutable(elf, 0xe0000808).status().code(),
            absl::StatusCode::kDataLoss);
}

TEST(E32Test, PreservesIndependentDataBssAndTypedPointers) {
  const auto image =
      ConvertPicExecutable(testing::DataExecutable(), 0xe0000808);
  ASSERT_TRUE(image.ok()) << image.status();
  const auto info = InspectImage(*image);
  ASSERT_TRUE(info.ok()) << info.status();
  EXPECT_EQ(info->data_size, 12);
  EXPECT_EQ(info->bss_size, 12);
  EXPECT_EQ(info->data_base, 0x20000000);
  EXPECT_EQ(info->code_relocations, std::vector<uint32_t>{16});
  EXPECT_EQ(info->code_data_relocations, std::vector<uint32_t>{16});
  EXPECT_EQ(info->data_relocations, (std::vector<uint32_t>{0, 4, 8}));
  EXPECT_EQ(info->data_data_relocations, (std::vector<uint32_t>{4, 8}));
  const auto data_offset = Read32(*image, 104);
  EXPECT_EQ(Read32(*image, data_offset), 0x8019);  // Preserve Thumb bit.
  EXPECT_EQ(Read32(*image, data_offset + 8), 0x2000000c);
  for (size_t i = 0; i < image->size(); ++i) {
    EXPECT_FALSE(InspectImage(std::string_view(*image).substr(0, i)).ok()) << i;
  }
  for (const auto offset :
       {size_t{172}, size_t{data_offset}, size_t{data_offset + 4}}) {
    auto changed = *image;
    Put32(changed, offset, 0x10000000);
    EXPECT_FALSE(InspectImage(changed).ok()) << offset;
  }
  auto changed = *image;
  // Claim a code target for a word that actually points into data.
  Put16(changed, Read32(changed, 112) + 16, 0x1010);
  EXPECT_FALSE(InspectImage(changed).ok());
}

TEST(E32Test, AcceptsBssWithoutInitializedData) {
  auto elf = testing::DataExecutable();
  Put32(elf, 100, 0);           // RW file size.
  Put32(elf, 340, 0);           // Empty data section.
  Put32(elf, 372, 0x20000000);  // BSS address.
  Put32(elf, 380, 24);
  Put32(elf, 420, 0);  // No retained data-source relocations.
  Put16(elf, 526, 5);  // Symbol 2 now denotes BSS.
  const auto image = ConvertPicExecutable(elf, 0xe0000808);
  ASSERT_TRUE(image.ok()) << image.status();
  const auto info = InspectImage(*image);
  ASSERT_TRUE(info.ok()) << info.status();
  EXPECT_EQ(info->data_size, 0);
  EXPECT_EQ(info->bss_size, 24);
  EXPECT_EQ(Read32(*image, 104), 0);
  EXPECT_TRUE(info->data_relocations.empty());
  EXPECT_EQ(info->code_data_relocations, std::vector<uint32_t>{16});
}

TEST(E32Test, RejectsMalformedWritableMappingsAndUnverifiedLifetime) {
  for (const auto& [offset, value] : std::vector<std::pair<size_t, uint32_t>>{
           {88, 116},
           {92, 0x8000},
           {92, 0xfffffffc},
           {100, 13},
           {104, 8},
           {104, 1024 * 1024 + 4},
           {108, 7},
           {112, 3},
           {328, 0x403},
           {324, 14},
           {372, 0x20000008},
           {548, 0x203},       // Cross-mapping REL32.
           {552, 0x2000000c},  // Relocation source is BSS.
           {508,
            2},  // Function symbol in data (symbol 1 changed below instead).
       }) {
    auto elf = testing::DataExecutable();
    Put32(elf, offset, value);
    if (offset == 508) {
      Put32(elf, 500, 0x20000000);
    }
    EXPECT_FALSE(ConvertPicExecutable(elf, 0xe0000808).ok()) << offset;
  }
}

TEST(E32Test, EncodesTypedFixupsWithoutWeakeningLegacyDecode) {
  const std::vector<uint32_t> offsets{0, 4, 4096};
  const std::set<uint32_t> data{4};
  const auto encoded = internal::EncodeCodeRelocations(offsets, data);
  ASSERT_TRUE(encoded.ok()) << encoded.status();
  EXPECT_FALSE(internal::DecodeCodeRelocations(*encoded, 4100).ok());
  std::set<uint32_t> decoded_data;
  auto decoded = internal::DecodeCodeRelocations(*encoded, 4100, &decoded_data);
  ASSERT_TRUE(decoded.ok()) << decoded.status();
  EXPECT_EQ(*decoded, offsets);
  EXPECT_EQ(decoded_data, data);
  EXPECT_FALSE(internal::EncodeCodeRelocations(offsets, {8}).ok());
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

// LLD synthesizes local GOT words without emitting ABS32 records for them.
// Retained GOT_PREL references identify the symbols whose words need fixups.
std::string GotExecutable() {
  std::string elf = Executable();
  const std::string sections = elf.substr(116, 200);
  elf.resize(512 + 7 * 40, '\0');
  elf.replace(512, sections.size(), sections);
  Put32(elf, 32, 512);
  Put16(elf, 48, 7);
  Put16(elf, 50, 5);
  Put32(elf, 512 + 40 + 20, 24);  // Text excludes the GOT.
  Put32(elf, 336, 0x8014);
  elf[344] = 0x11;         // Global object.
  Put32(elf, 352, 0x160);  // R_ARM_GOT_PREL, symbol 1.
  Put32(elf, 100, 8);      // Resolved PC-relative displacement stays unchanged.
  Put32(elf, 108, 0x8014);
  elf.replace(356, 6, "\0.got\0", 6);
  Put32(elf, 512 + 5 * 40 + 4, 3);  // Section-name string table.
  Put32(elf, 512 + 5 * 40 + 16, 356);
  Put32(elf, 512 + 5 * 40 + 20, 6);
  const size_t got = 512 + 6 * 40;
  Put32(elf, got, 1);
  Put32(elf, got + 4, 1);
  Put32(elf, got + 8, 3);
  Put32(elf, got + 12, 0x8018);
  Put32(elf, got + 16, 108);
  Put32(elf, got + 20, 4);
  return elf;
}

TEST(E32GotTest, RelocatesLocalObjectWordAndPreservesLinkedReference) {
  const auto image = ConvertPicExecutable(GotExecutable(), 0xe0000808);
  ASSERT_TRUE(image.ok()) << image.status();
  const auto info = InspectImage(*image);
  ASSERT_TRUE(info.ok()) << info.status();
  EXPECT_EQ(info->code_relocations, std::vector<uint32_t>{24});
  EXPECT_EQ(Read32(*image, 156 + 24), 0x8014);
  EXPECT_EQ(Read32(*image, 156 + 16), 8);
}

TEST(E32GotTest, PreservesThumbAndOddByteObjectAddressesExactly) {
  for (const char type : {char{0x12}, char{0x11}}) {
    std::string elf = GotExecutable();
    elf[344] = type;
    Put32(elf, 336, 0x8015);
    Put32(elf, 108, 0x8015);
    const auto image = ConvertPicExecutable(elf, 0xe0000808);
    ASSERT_TRUE(image.ok()) << image.status();
    EXPECT_EQ(Read32(*image, 156 + 24), 0x8015);
    Put32(elf, 108, 0x8014);
    EXPECT_EQ(ConvertPicExecutable(elf, 0xe0000808).status().code(),
              absl::StatusCode::kDataLoss);
  }
}

TEST(E32GotTest, RejectsMissingTableUndefinedSymbolAndUnclaimedWords) {
  std::string elf = GotExecutable();
  Put32(elf, 512 + 6 * 40 + 20, 0);
  EXPECT_FALSE(ConvertPicExecutable(elf, 0xe0000808).ok());
  elf = GotExecutable();
  Put16(elf, 346, 0);
  EXPECT_EQ(ConvertPicExecutable(elf, 0xe0000808).status().code(),
            absl::StatusCode::kUnimplemented);
  for (uint32_t value : {0U, 0x8016U, 0xfffffff0U}) {
    elf = GotExecutable();
    Put32(elf, 108, value);
    EXPECT_EQ(ConvertPicExecutable(elf, 0xe0000808).status().code(),
              absl::StatusCode::kDataLoss);
  }
  elf = GotExecutable();
  Put32(elf, 352, 0x11c);  // Ordinary call cannot claim a GOT word.
  EXPECT_EQ(ConvertPicExecutable(elf, 0xe0000808).status().code(),
            absl::StatusCode::kDataLoss);
}

TEST(E32GotTest, CoversEverySlotAndRejectsMissingSymbolCoverage) {
  std::string elf = GotExecutable();
  Put32(elf, 512 + 6 * 40 + 20, 8);
  Put32(elf, 112, 0x8014);  // Two aliases still need two loader fixups.
  const auto image = ConvertPicExecutable(elf, 0xe0000808);
  ASSERT_TRUE(image.ok()) << image.status();
  EXPECT_EQ(InspectImage(*image)->code_relocations,
            (std::vector<uint32_t>{24, 28}));
  Put32(elf, 112, 0x8016);
  EXPECT_EQ(ConvertPicExecutable(elf, 0xe0000808).status().code(),
            absl::StatusCode::kDataLoss);
  elf = GotExecutable();
  // A second referenced symbol without its own GOT word cannot be published.
  Put32(elf, 320, 0x8016);  // Symbol zero becomes another defined object.
  elf[328] = 0x11;
  Put16(elf, 330, 1);
  const size_t relocs = elf.size();
  elf.append(16, '\0');
  Put32(elf, relocs, 0x8010);
  Put32(elf, relocs + 4, 0x160);
  Put32(elf, relocs + 8, 0x8014);
  Put32(elf, relocs + 12, 0x60);
  Put32(elf, 512 + 3 * 40 + 16, static_cast<uint32_t>(relocs));
  Put32(elf, 512 + 3 * 40 + 20, 16);
  EXPECT_EQ(ConvertPicExecutable(elf, 0xe0000808).status().code(),
            absl::StatusCode::kDataLoss);
}

TEST(E32GotTest, RejectsSymbolsOutsideTheirSectionAndUnsupportedSymbolKinds) {
  std::string elf = GotExecutable();
  Put32(elf, 336, 0x801c);  // In RX, but outside the symbol's text section.
  Put32(elf, 108, 0x801c);
  EXPECT_EQ(ConvertPicExecutable(elf, 0xe0000808).status().code(),
            absl::StatusCode::kUnimplemented);
  elf = GotExecutable();
  elf[344] = 0x10;  // Undefined symbol semantics must not be guessed.
  EXPECT_EQ(ConvertPicExecutable(elf, 0xe0000808).status().code(),
            absl::StatusCode::kUnimplemented);
}

TEST(E32GotTest, RejectsBadShapeNamesDuplicateTablesAndTargetRelocations) {
  const size_t got = 512 + 6 * 40;
  for (const size_t offset : {got, got + 4, got + 8, got + 12, got + 16,
                              got + 20, size_t{512 + 5 * 40 + 16}}) {
    std::string elf = GotExecutable();
    Put32(elf, offset, 0xfffffff0);
    EXPECT_FALSE(ConvertPicExecutable(elf, 0xe0000808).ok()) << offset;
  }
  std::string elf = GotExecutable();
  Put32(elf, got + 20, 3);
  EXPECT_FALSE(ConvertPicExecutable(elf, 0xe0000808).ok());
  elf = GotExecutable();
  elf.append(elf.substr(got, 40));
  Put16(elf, 48, 8);
  EXPECT_EQ(ConvertPicExecutable(elf, 0xe0000808).status().code(),
            absl::StatusCode::kDataLoss);
  elf = GotExecutable();
  Put32(elf, 512 + 3 * 40 + 28, 6);  // Relocation target is the GOT itself.
  Put32(elf, 348, 0x8018);
  EXPECT_EQ(ConvertPicExecutable(elf, 0xe0000808).status().code(),
            absl::StatusCode::kUnimplemented);
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

std::string ExportedDataExecutable() {
  std::string elf = testing::DataExecutable();
  // Name the existing Thumb function symbol without changing the data and
  // pointer-relocation fixture.
  Put32(elf, 456, static_cast<uint32_t>(elf.size()));
  Put32(elf, 460, 10);
  elf.append("\0Function\0", 10);
  Put32(elf, 496, 1);
  Put32(elf, 504, 4);
  elf[508] = 0x12;
  return elf;
}

TEST(E32DllTest, PreservesWritableDataBssAndTypedRelocations) {
  const auto image =
      ConvertDll(ExportedDataExecutable(), "EXPORTS\nFunction @ 1 NONAME\n", {},
                 0xe0000810);
  ASSERT_TRUE(image.ok()) << image.status();
  const auto info = InspectImage(*image);
  ASSERT_TRUE(info.ok()) << info.status();
  EXPECT_TRUE(info->dll);
  EXPECT_EQ(info->data_size, 12);
  EXPECT_EQ(info->bss_size, 12);
  EXPECT_EQ(info->data_base, 0x20000000);
  EXPECT_EQ(info->code_relocations, (std::vector<uint32_t>{16, 36}));
  EXPECT_EQ(info->code_data_relocations, (std::vector<uint32_t>{16}));
  EXPECT_EQ(info->data_relocations, (std::vector<uint32_t>{0, 4, 8}));
  EXPECT_EQ(info->data_data_relocations, (std::vector<uint32_t>{4, 8}));
  EXPECT_EQ(info->exports.front().address, 0x8019);
  EXPECT_EQ(*image,
            *ConvertDll(ExportedDataExecutable(),
                        "EXPORTS\nFunction @ 1 NONAME\n", {}, 0xe0000810));
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
