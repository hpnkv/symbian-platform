#ifndef SYMBIAN_E32_E32_H_
#define SYMBIAN_E32_E32_H_

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <absl/status/statusor.h>

namespace symbian::e32 {

struct ImportSlot {
  uint32_t code_offset = 0;
  uint32_t ordinal = 0;
};

struct ImportBlock {
  std::string dll;
  std::vector<ImportSlot> slots;
};

struct ExportSlot {
  uint32_t ordinal = 0;
  uint32_t address = 0;
  bool absent = false;
};

// Metadata for the narrow, uncompressed experimental profiles.
// Successful inspection does not prove acceptance by a device loader.
struct ImageInfo {
  uint32_t uid3 = 0;
  uint32_t header_crc = 0;
  uint32_t flags = 0;
  uint32_t code_size = 0;
  uint32_t code_base = 0;
  uint32_t entry_offset = 0;
  uint32_t secure_id = 0;
  bool dll = false;
  uint32_t header_size = 0;
  std::vector<ImportBlock> imports;
  std::vector<ExportSlot> exports;
  std::vector<uint32_t> code_relocations;
};

// Accepts ARM EABI5 ET_EXEC linked with --emit-relocs: one RX PT_LOAD,
// internal relative references and resolved ABS32 pointers into RX code,
// EKA2 ARM entry, no imports/writable application data/exports.
// Named .data.rel.ro tables are placed in the read-only code mapping.
// Input must come from a trusted link retaining ALL relocations. This cannot
// detect stripped relocations or absolute addresses hand-written in code.
// UID3 must be in the experimental unprotected 0xe0000000..0xefffffff range.
absl::StatusOr<std::string> ConvertPicExecutable(std::string_view elf,
                                                 uint32_t uid3);

// Eager function imports from validated ordinal proxies, retained call relocs,
// one RX load containing GOT/PLT and dynamic metadata. No data/TLS/constructors.
// Link with --emit-relocs and the import layout; all imports have zero addends.
absl::StatusOr<std::string> ConvertImportedExecutable(
    std::string_view elf, const std::vector<std::string>& proxies,
    uint32_t uid3);

// Frozen function exports from a trusted retained-relocation EKA2 PIC image.
// Emits count word, complete ordinal table, absence bitmap and code relocations
// for every slot.
// Optional eager function imports use the same profile as the executable path.
absl::StatusOr<std::string> ConvertDll(std::string_view elf,
                                       std::string_view definition,
                                       const std::vector<std::string>& proxies,
                                       uint32_t uid3);

// Checks bounds, UID checksum, header CRC and this supported profile only.
// Other E32 profiles return Unimplemented, not a general validity verdict.
absl::StatusOr<ImageInfo> InspectImage(std::string_view bytes);

}  // namespace symbian::e32

#endif  // SYMBIAN_E32_E32_H_
