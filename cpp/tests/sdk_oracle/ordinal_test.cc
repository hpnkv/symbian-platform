#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>

#include <absl/base/nullability.h>
#include <gtest/gtest.h>

#include "ordinal_types.h"
#include "symbian/analysis/bytes.h"
#include "symbian/sdk/exports.h"

namespace {

TEST(SdkOrdinalOracleTest, NokiaMethodReadsUserExitOrdinalFromClangLldProxy) {
  const char* absl_nullable path = std::getenv("SYMBIAN_SDK_PROXY_TEST_IMAGE");
  ASSERT_NE(path, nullptr);
  std::ifstream file(path, std::ios::binary);
  ASSERT_TRUE(file.is_open());
  std::string bytes{std::istreambuf_iterator<char>(file),
                    std::istreambuf_iterator<char>()};
  // This historical method is a trusted-fixture oracle, not an untrusted API.
  const auto info = symbian::sdk::InspectProxy(bytes);
  ASSERT_TRUE(info.ok()) << info.status();
  using symbian::analysis::internal::Read16;
  using symbian::analysis::internal::Read32;
  const size_t program = Read32(bytes, 28);
  Elf32_Phdr code{Read32(bytes, program),      Read32(bytes, program + 4),
                  Read32(bytes, program + 8),  Read32(bytes, program + 12),
                  Read32(bytes, program + 16), Read32(bytes, program + 20),
                  Read32(bytes, program + 24), Read32(bytes, program + 28)};
  ElfExecutable historical;
  historical.iElfHeader = reinterpret_cast<Elf32_Ehdr*>(bytes.data());
  historical.iCodeSegmentHdr = &code;
  bool found = false;
  for (size_t i = 0; i < Read16(bytes, 48); ++i) {
    const size_t section = Read32(bytes, 32) + i * Read16(bytes, 46);
    if (Read32(bytes, section + 4) != 11) {
      continue;
    }
    const size_t offset = Read32(bytes, section + 16);
    const size_t size = Read32(bytes, section + 20);
    for (size_t p = offset + 16; p < offset + size; p += 16) {
      Elf32_Sym symbol{Read32(bytes, p),
                       Read32(bytes, p + 4),
                       Read32(bytes, p + 8),
                       static_cast<uint8_t>(bytes[p + 12]),
                       static_cast<uint8_t>(bytes[p + 13]),
                       Read16(bytes, p + 14)};
      EXPECT_EQ(historical.GetSymbolOrdinal(&symbol), 641);
      symbol.st_shndx = 2;
      EXPECT_EQ(historical.GetSymbolOrdinal(&symbol), UINT32_MAX);
      found = true;
    }
  }
  EXPECT_TRUE(found);
  ASSERT_EQ(info->exports.size(), 1);
  EXPECT_EQ(info->exports[0].symbol, "_ZN4User4ExitEi");
  EXPECT_EQ(info->exports[0].ordinal, 641);
}

}  // namespace
