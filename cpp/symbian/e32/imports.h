#ifndef SYMBIAN_E32_IMPORTS_H_
#define SYMBIAN_E32_IMPORTS_H_

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <absl/status/statusor.h>

#include "symbian/e32/e32.h"

namespace symbian::e32::internal {

struct Segment {
  uint32_t offset = 0;
  uint32_t address = 0;
  uint32_t size = 0;
};

struct Section {
  uint32_t type;
  uint32_t flags;
  uint32_t address;
  uint32_t offset;
  uint32_t size;
  uint32_t link;
  uint32_t info;
  uint32_t entry_size;
  std::string name;
};

struct ResolvedImports {
  std::vector<ImportBlock> blocks;
  std::map<std::string, uint32_t> functions;
  std::map<std::string, uint32_t> objects;
  std::set<uint32_t> object_slots;
  std::map<std::string, uint32_t> plt_functions;
  std::map<uint32_t, uint32_t> data_function_pointers;
  size_t got_index = 0;
  size_t dynamic_index = 0;
  size_t relocation_index = 0;
  size_t data_relocation_index = 0;
};

absl::StatusOr<ResolvedImports> ResolveImports(
    std::string_view elf, const std::vector<Section>& sections,
    const Segment& code, const std::vector<std::string>& proxies);

absl::StatusOr<std::string> SymbolName(std::string_view elf,
                                       const std::vector<Section>& sections,
                                       const Section& symbols, size_t symbol);

absl::Status CheckImportCall(std::string_view elf, const Segment& code,
                             uint32_t location, uint32_t type,
                             uint32_t slot_offset);

std::string EncodeImports(const std::vector<ImportBlock>& imports);
absl::StatusOr<std::vector<ImportBlock>> DecodeImports(std::string_view section,
                                                       std::string_view code,
                                                       uint32_t count);

// Legacy EKA1 PE imports: block words are ordinals, and loader-owned slots
// occupy a contiguous, zero-terminated IAT immediately after text_size.
std::string EncodePeImports(const std::vector<ImportBlock>& imports);
absl::StatusOr<std::vector<ImportBlock>> DecodePeImports(
    std::string_view section, std::string_view code, uint32_t text_size,
    uint32_t count);

}  // namespace symbian::e32::internal

#endif  // SYMBIAN_E32_IMPORTS_H_
