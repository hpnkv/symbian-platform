#include "symbian/e32/e32.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <absl/status/status.h>

#include "symbian/analysis/bytes.h"
#include "symbian/analysis/checksum.h"
#include "symbian/analysis/elf.h"

namespace symbian::e32 {
namespace {

using analysis::internal::Put16;
using analysis::internal::Put32;
using analysis::internal::Read16;
using analysis::internal::Read32;
using analysis::internal::UidChecksum;
using analysis::internal::Within;

constexpr uint32_t kHeaderSize = 156;
constexpr uint32_t kFlags = 0x12000028;  // V header, ELF imports, EABI, EKA2.
constexpr uint32_t kCrcInitializer = 0xc90fdaa2;
constexpr uint32_t kMaxImageSize = 0x0fffffff;

// Symbian CRC32 uses reflected polynomial 0xedb88320, initial zero and no
// final complement. UID CRC16 uses polynomial 0x1021 and initial zero.
uint32_t HeaderCrc(std::string_view bytes) {
  uint32_t crc = 0;
  for (size_t i = 0; i < kHeaderSize; ++i) {
    const uint8_t byte =
        i >= 20 && i < 24
            ? static_cast<uint8_t>(kCrcInitializer >> ((i - 20) * 8))
            : static_cast<uint8_t>(bytes[i]);
    crc ^= byte;
    for (int bit = 0; bit < 8; ++bit) {
      crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320 : 0);
    }
  }
  return crc;
}

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
};

absl::Status CheckRelocations(std::string_view elf,
                              const std::vector<Section>& sections,
                              const Segment& code) {
  bool retained = false;
  for (const Section& section : sections) {
    if (section.type != 9 || section.size == 0) {  // SHT_REL
      continue;
    }
    if (section.entry_size != 8 || section.size % 8 != 0 ||
        section.link >= sections.size() || section.info >= sections.size()) {
      return absl::DataLossError("Invalid ELF relocation table");
    }
    const Section& symbols = sections[section.link];
    const Section& target = sections[section.info];
    if (symbols.type != 2 || symbols.entry_size != 16 ||
        symbols.size % 16 != 0) {
      return absl::DataLossError("Invalid ELF relocation symbol table");
    }
    if (!(target.flags & 2)) {
      continue;  // Debug relocations do not become runtime references.
    }
    retained = true;
    for (size_t i = 0; i < section.size; i += 8) {
      const uint32_t location = Read32(elf, section.offset + i);
      const uint32_t info = Read32(elf, section.offset + i + 4);
      const uint32_t symbol = info >> 8;
      const uint32_t type = info & 0xff;
      // ARM ABI PC-relative relocations only. No ABS32, MOVW/MOVT, GOT,
      // dynamic relocations or imported symbols are silently discarded.
      if (type != 1 && type != 3 && type != 10 && type != 28 && type != 29 &&
          type != 30 && type != 42) {
        return absl::UnimplementedError("Unsupported ARM relocation type");
      }
      if (location < target.address ||
          !Within(target.size, location - target.address, 4) ||
          symbol >= symbols.size / 16) {
        return absl::DataLossError("ELF relocation is outside its target");
      }
      const size_t sym = symbols.offset + static_cast<size_t>(symbol) * 16;
      const uint16_t index = Read16(elf, sym + 14);
      const uint32_t address = Read32(elf, sym + 4) & ~uint32_t{1};
      if (index == 0 || index >= sections.size() ||
          !(sections[index].flags & 2) || address < code.address ||
          !Within(code.size, address - code.address, 1)) {
        return absl::UnimplementedError("Relocation needs an external symbol");
      }
    }
  }
  if (!retained) {
    return absl::FailedPreconditionError(
        "Retained runtime relocation records required; link with "
        "--emit-relocs");
  }
  return absl::OkStatus();
}

absl::StatusOr<Segment> ExtractCode(std::string_view elf,
                                    const analysis::Elf32Header& header) {
  if (header.type != 2 || header.machine != 40 || header.flags >> 24 != 5 ||
      (header.flags & 0x400)) {
    return absl::UnimplementedError("Requires ARM EABI5 soft-float ET_EXEC");
  }
  Segment code;
  bool found = false;
  for (size_t i = 0; i < header.program_count; ++i) {
    const size_t p = Read32(elf, 28) + i * Read16(elf, 42);
    const uint32_t type = Read32(elf, p);
    if (type == 0x70000001 || type == 0x6474e551) {
      continue;  // ARM unwind index, GNU stack; no dynamic loader contract.
    }
    if (type != 1 || found || Read32(elf, p + 24) != 5 ||
        Read32(elf, p + 16) != Read32(elf, p + 20)) {
      return absl::UnimplementedError("Requires exactly one RX load segment");
    }
    found = true;
    code = {Read32(elf, p + 4), Read32(elf, p + 8), Read32(elf, p + 16)};
    const uint32_t alignment = Read32(elf, p + 28);
    if (!Within(elf.size(), code.offset, code.size) || code.address % 4 ||
        code.size < 16 || code.size > kMaxImageSize - kHeaderSize ||
        code.size > UINT32_MAX - code.address ||
        (alignment > 1 &&
         ((alignment & (alignment - 1)) ||
          code.offset % alignment != code.address % alignment))) {
      return absl::DataLossError("Invalid ELF code segment bounds/alignment");
    }
  }
  if (!found) {
    return absl::DataLossError("Missing ELF code segment");
  }
  std::vector<Section> sections;
  for (size_t i = 0; i < header.section_count; ++i) {
    const size_t s = Read32(elf, 32) + i * Read16(elf, 46);
    Section section{Read32(elf, s + 4),  Read32(elf, s + 8),
                    Read32(elf, s + 12), Read32(elf, s + 16),
                    Read32(elf, s + 20), Read32(elf, s + 24),
                    Read32(elf, s + 28), Read32(elf, s + 36)};
    if (section.type != 8 &&
        !Within(elf.size(), section.offset, section.size)) {
      return absl::DataLossError("ELF section outside file");
    }
    if (section.type == 4 || section.type == 6 || section.type == 11) {
      return absl::UnimplementedError("Dynamic/RELA ELF is unsupported");
    }
    if ((section.flags & 2) && section.size) {
      if ((section.flags & 0x401) || section.type == 8 ||
          (section.type >= 14 && section.type <= 16)) {
        return absl::UnimplementedError("Data/TLS/constructors unsupported");
      }
      if (section.address < code.address || section.offset < code.offset ||
          !Within(code.size, section.address - code.address, section.size) ||
          section.offset - code.offset != section.address - code.address) {
        return absl::DataLossError("Allocated section outside code segment");
      }
    }
    sections.push_back(section);
  }
  const absl::Status status = CheckRelocations(elf, sections, code);
  if (!status.ok()) {
    return status;
  }
  return code;
}

absl::Status CheckEntry(std::string_view code, uint32_t entry) {
  if (entry % 4 || !Within(code.size(), entry, 16)) {
    return absl::DataLossError("EKA2 ARM entry is unaligned or outside code");
  }
  if (Read32(code, entry) != 0xe31f0000 || Read32(code, entry + 12) != 0) {
    return absl::FailedPreconditionError(
        "EKA2 startup marker/reserved code-segment word missing");
  }
  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<std::string> ConvertPicExecutable(std::string_view elf,
                                                 uint32_t uid3) {
  if (uid3 < 0xe0000000 || uid3 > 0xefffffff) {
    return absl::InvalidArgumentError("Experimental unprotected UID3 required");
  }
  const auto header = analysis::InspectElf32(elf);
  if (!header.ok()) {
    return header.status();
  }
  const auto segment = ExtractCode(elf, *header);
  if (!segment.ok()) {
    return segment.status();
  }
  if (header->entry < segment->address) {
    return absl::DataLossError("ELF entry precedes code");
  }
  const uint32_t entry = header->entry - segment->address;
  const std::string_view code = elf.substr(segment->offset, segment->size);
  const absl::Status status = CheckEntry(code, entry);
  if (!status.ok()) {
    return status;
  }
  std::string bytes(kHeaderSize, '\0');
  Put32(bytes, 0, 0x1000007a);  // Executable image UID1.
  Put32(bytes, 8, uid3);
  Put32(bytes, 12, UidChecksum(bytes));
  Put32(bytes, 16, 0x434f5045);  // EPOC
  Put32(bytes, 24, 0x00010000);  // Module version 1.0.
  bytes[33] = 1;                 // Converter version 0.1.0.
  Put32(bytes, 44, kFlags);
  Put32(bytes, 48, segment->size);
  Put32(bytes, 56, 0x1000);
  Put32(bytes, 60, 0x100000);
  Put32(bytes, 64, 0x10000);
  Put32(bytes, 72, entry);
  Put32(bytes, 76, segment->address);
  Put32(bytes, 96, segment->size);
  Put32(bytes, 100, kHeaderSize);
  Put16(bytes, 120, 350);     // Foreground process priority.
  Put16(bytes, 122, 0x2001);  // ARMv5.
  Put32(bytes, 124, segment->size);
  Put32(bytes, 128, uid3);  // Secure ID; no capabilities or vendor ID.
  Put32(bytes, 20, HeaderCrc(bytes));
  bytes.append(code);
  return bytes;
}

absl::StatusOr<ImageInfo> InspectImage(std::string_view bytes) {
  if (bytes.size() < kHeaderSize || bytes.size() > kMaxImageSize ||
      bytes.substr(16, 4) != "EPOC") {
    return absl::DataLossError("Missing/truncated E32 V header");
  }
  if (Read32(bytes, 44) != kFlags || Read32(bytes, 28) != 0 ||
      Read32(bytes, 100) != kHeaderSize || Read16(bytes, 122) != 0x2001) {
    return absl::UnimplementedError("Unsupported E32 image profile");
  }
  if (Read32(bytes, 0) != 0x1000007a || Read32(bytes, 4) != 0 ||
      Read32(bytes, 12) != UidChecksum(bytes) ||
      Read32(bytes, 20) != HeaderCrc(bytes)) {
    return absl::DataLossError("Invalid E32 identity/header checksum");
  }
  for (size_t offset :
       {size_t{52}, size_t{68}, size_t{80}, size_t{84}, size_t{88}, size_t{92},
        size_t{104}, size_t{108}, size_t{112}, size_t{116}, size_t{132},
        size_t{136}, size_t{140}, size_t{144}, size_t{148}, size_t{152}}) {
    if (Read32(bytes, offset) != 0) {
      return absl::UnimplementedError("E32 data/imports/exports unsupported");
    }
  }
  const uint32_t size = Read32(bytes, 48);
  const uint32_t uid3 = Read32(bytes, 8);
  if (size != bytes.size() - kHeaderSize || Read32(bytes, 96) != size ||
      Read32(bytes, 124) != size || Read32(bytes, 76) % 4 ||
      size > UINT32_MAX - Read32(bytes, 76) || uid3 < 0xe0000000 ||
      uid3 > 0xefffffff || Read32(bytes, 128) != uid3 ||
      Read32(bytes, 24) != 0x00010000 || Read32(bytes, 56) != 0x1000 ||
      Read32(bytes, 60) != 0x100000 || Read32(bytes, 64) != 0x10000) {
    return absl::DataLossError("Invalid experimental E32 layout/identity");
  }
  const absl::Status status =
      CheckEntry(bytes.substr(kHeaderSize), Read32(bytes, 72));
  if (!status.ok()) {
    return status;
  }
  return ImageInfo{.uid3 = uid3,
                   .header_crc = Read32(bytes, 20),
                   .flags = kFlags,
                   .code_size = size,
                   .code_base = Read32(bytes, 76),
                   .entry_offset = Read32(bytes, 72),
                   .secure_id = Read32(bytes, 128)};
}

}  // namespace symbian::e32
