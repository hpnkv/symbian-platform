// Oracle for Nokia's unchanged EPL-1.0 f32image.h implementation.

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>

#include <gtest/gtest.h>

#include "validator_host_types.h"

#define INCLUDE_E32IMAGEHEADER_IMPLEMENTATION
#define __CPU_ARM
#define __EABI__
#include <f32image.h>

namespace {

static_assert(sizeof(E32ImageHeader) == 124);
static_assert(sizeof(E32ImageHeaderComp) == 128);
static_assert(sizeof(E32ImageHeaderV) == 156);
static_assert(offsetof(E32ImageHeaderV, iExportDescSize) == 152);

class ValidatorOracleTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const char* path = std::getenv("SYMBIAN_E32_TEST_IMAGE");
    ASSERT_NE(path, nullptr);
    std::ifstream stream(path, std::ios::binary);
    ASSERT_TRUE(stream.is_open()) << path;
    bytes_.assign(std::istreambuf_iterator<char>(stream),
                  std::istreambuf_iterator<char>());
    ASSERT_GE(bytes_.size(), sizeof(header_));
    ASSERT_LT(bytes_.size(), 0x0fffffffU);
    std::memcpy(&header_, bytes_.data(), sizeof(header_));
    header_size_ = header_.iCodeOffset;
    ASSERT_GE(header_size_, sizeof(header_));
    ASSERT_LE(header_size_, sizeof(storage_));
    ASSERT_LE(header_size_, bytes_.size());
    std::memcpy(&storage_, bytes_.data(), header_size_);
    ASSERT_EQ(header_.iCompressionType, 0);
  }

  TInt Validate() {
    // These tests use a bounded, uncompressed V fixture. Do not expose this
    // historical pointer-based validator as an untrusted-file execution API.
    return header_.ValidateWholeImage(
        bytes_.data() + header_size_,
        static_cast<TUint>(bytes_.size() - header_size_));
  }

  void UpdateHeaderCrc() {
    header_.iHeaderCrc = KImageCrcInitialiser;
    TUint32 crc = 0;
    Mem::Crc32(crc, &storage_, header_size_);
    header_.iHeaderCrc = crc;
  }

  // Original Symbian variable header extends the export bitmap past sizeof(V).
  // Keep its complete, bounded contiguous backing storage in this test adapter.
  struct HeaderStorage {
    E32ImageHeaderV header{};
    char continuation[8192]{};
  } storage_;

  static_assert(offsetof(HeaderStorage, continuation) == 156);
  E32ImageHeaderV& header_ = storage_.header;
  uint32_t header_size_ = 0;
  std::string bytes_;
};

TEST_F(ValidatorOracleTest, AcceptsCompleteNativeConverterImage) {
  EXPECT_EQ(Validate(), KErrNone);
}

TEST_F(ValidatorOracleTest, RejectsBadHeaderCrc) {
  header_.iHeaderCrc ^= 1;
  EXPECT_EQ(Validate(), KErrCorrupt);
}

TEST_F(ValidatorOracleTest, RejectsNegativeHeapDespiteCorrectCrc) {
  header_.iHeapSizeMin = -1;
  UpdateHeaderCrc();
  EXPECT_EQ(Validate(), KErrCorrupt);
}

TEST_F(ValidatorOracleTest, RequiresSpaceForEntryCodeSegmentId) {
  header_.iEntryPoint = static_cast<TUint>(header_.iCodeSize) - 8;
  UpdateHeaderCrc();
  EXPECT_EQ(Validate(), KErrCorrupt);
}

TEST_F(ValidatorOracleTest, RejectsMissingImportSectionDespiteCorrectCrc) {
  header_.iDllRefTableCount = 1;
  header_.iImportOffset = 0;
  UpdateHeaderCrc();
  EXPECT_EQ(Validate(), KErrCorrupt);
}

TEST_F(ValidatorOracleTest, RequiresArmEabiEntryContract) {
  header_.iFlags &= ~KImageABIMask;
  UpdateHeaderCrc();
  EXPECT_EQ(Validate(), KErrNotSupported);
}

TEST_F(ValidatorOracleTest, RejectsTruncatedCode) {
  bytes_.pop_back();
  EXPECT_EQ(Validate(), KErrCorrupt);
}

}  // namespace
