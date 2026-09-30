#include "symbian/analysis/elf.h"

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <absl/status/status.h>
#include <absl/strings/str_cat.h>

#include "symbian/analysis/bytes.h"

namespace symbian::analysis {
namespace {

using internal::Read16;
using internal::Read32;

absl::Status CheckTable(size_t size, uint32_t offset, uint16_t width,
                        uint16_t count, uint16_t minimum,
                        std::string_view name) {
  if (count == 0) {
    return absl::OkStatus();
  }
  if (offset < 52 || width < minimum || offset > size ||
      static_cast<uint64_t>(width) * count > size - offset) {
    return absl::DataLossError(
        absl::StrCat(name, " table is outside ELF file"));
  }
  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<Elf32Header> InspectElf32(std::string_view bytes) {
  if (bytes.size() < 16 || bytes.substr(0, 4) !=
                               "\x7f"
                               "ELF") {
    return absl::DataLossError("Missing or truncated ELF identification");
  }
  if (bytes[4] != 1 || bytes[5] != 1) {
    return absl::UnimplementedError("Only ELF32 little-endian is supported");
  }
  if (bytes.size() < 52 || bytes[6] != 1 || Read32(bytes, 20) != 1 ||
      Read16(bytes, 40) != 52) {
    return absl::DataLossError("Invalid ELF32 header size or version");
  }
  const uint32_t program_offset = Read32(bytes, 28);
  const uint32_t section_offset = Read32(bytes, 32);
  const uint16_t program_count = Read16(bytes, 44);
  const uint16_t section_count = Read16(bytes, 48);
  const uint16_t string_section = Read16(bytes, 50);
  if (program_count == 0xffff || string_section == 0xffff ||
      (section_count == 0 && section_offset != 0)) {
    return absl::UnimplementedError("Extended ELF numbering is unsupported");
  }
  if (string_section != 0 && string_section >= section_count) {
    return absl::DataLossError("Invalid ELF section-name table index");
  }
  absl::Status status =
      CheckTable(bytes.size(), program_offset, Read16(bytes, 42), program_count,
                 32, "Program");
  if (!status.ok()) {
    return status;
  }
  status = CheckTable(bytes.size(), section_offset, Read16(bytes, 46),
                      section_count, 40, "Section");
  if (!status.ok()) {
    return status;
  }
  return Elf32Header{.type = Read16(bytes, 16),
                     .machine = Read16(bytes, 18),
                     .entry = Read32(bytes, 24),
                     .flags = Read32(bytes, 36),
                     .program_count = program_count,
                     .section_count = section_count};
}

}  // namespace symbian::analysis
