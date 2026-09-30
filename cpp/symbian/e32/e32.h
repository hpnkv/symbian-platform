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
  std::vector<ImportBlock> imports;
};

// Accepts ARM EABI5 ET_EXEC linked with --emit-relocs: one RX PT_LOAD,
// internal PC-relative references, EKA2 ARM entry, no imports/data/exports.
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

// Checks bounds, UID checksum, header CRC and this supported profile only.
// Other E32 profiles return Unimplemented, not a general validity verdict.
absl::StatusOr<ImageInfo> InspectImage(std::string_view bytes);

}  // namespace symbian::e32

#endif  // SYMBIAN_E32_E32_H_
