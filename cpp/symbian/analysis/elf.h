#ifndef SYMBIAN_ANALYSIS_ELF_H_
#define SYMBIAN_ANALYSIS_ELF_H_

#include <cstdint>
#include <string_view>

#include <absl/status/statusor.h>

namespace symbian::analysis {

// Metadata from an ELF32 little-endian header and bounded table locations.
// This is not a validator for section contents or Symbian loader acceptance.
struct Elf32Header {
  uint16_t type = 0;
  uint16_t machine = 0;
  uint32_t entry = 0;
  uint32_t flags = 0;
  uint16_t program_count = 0;
  uint16_t section_count = 0;
};

// Inspects a complete file. Extended numbering and other ELF classes/endian
// formats return Unimplemented; malformed supported headers return DataLoss.
absl::StatusOr<Elf32Header> InspectElf32(std::string_view bytes);

}  // namespace symbian::analysis

#endif  // SYMBIAN_ANALYSIS_ELF_H_
