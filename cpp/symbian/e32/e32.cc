#include "symbian/e32/e32.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <absl/status/status.h>

#include "symbian/analysis/bytes.h"
#include "symbian/analysis/checksum.h"
#include "symbian/analysis/elf.h"
#include "symbian/e32/exports.h"
#include "symbian/e32/imports.h"

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
uint32_t HeaderCrc(std::string_view bytes, size_t header_size) {
  uint32_t crc = 0;
  for (size_t i = 0; i < header_size; ++i) {
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

using internal::ResolvedImports;
using internal::Section;
using internal::Segment;

// ELF marks relocation-bearing const data writable before load-time fixups.
// This profile stores only named RELRO tables inside its RX code mapping.
absl::StatusOr<bool> IsRelro(std::string_view elf,
                             const analysis::Elf32Header& header,
                             size_t section_header, const Section& section) {
  if (section.type != 1 || (section.flags & 5) != 1) {
    return false;
  }
  const uint16_t names_index = Read16(elf, 50);
  if (names_index == 0 || names_index >= header.section_count) {
    return false;
  }
  const size_t names = Read32(elf, 32) + names_index * Read16(elf, 46);
  const uint32_t offset = Read32(elf, names + 16);
  const uint32_t size = Read32(elf, names + 20);
  const uint32_t name_offset = Read32(elf, section_header);
  if (Read32(elf, names + 4) != 3 || !Within(elf.size(), offset, size) ||
      name_offset >= size) {
    return absl::DataLossError("Invalid ELF RELRO section name table");
  }
  std::string_view name = elf.substr(offset + name_offset, size - name_offset);
  const size_t end = name.find('\0');
  if (end == std::string_view::npos) {
    return absl::DataLossError("Unterminated ELF section name");
  }
  name = name.substr(0, end);
  return name == ".data.rel.ro" || name.starts_with(".data.rel.ro.");
}

absl::Status CheckRelocations(std::string_view elf,
                              const std::vector<Section>& sections,
                              const Segment& code,
                              const ResolvedImports* imports,
                              std::vector<uint32_t>* pointers) {
  bool retained = false;
  for (size_t section_index = 0; section_index < sections.size();
       ++section_index) {
    const Section& section = sections[section_index];
    if (imports != nullptr && section_index == imports->relocation_index) {
      continue;
    }
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
      // PC-relative references and resolved ABS32 words into the RX mapping.
      // MOVW/MOVT, GOT and unknown dynamic contracts remain unsupported.
      if (type != 2 && type != 1 && type != 3 && type != 10 && type != 28 &&
          type != 29 && type != 30 && type != 42) {
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
      if (index == 0 && imports != nullptr) {
        const auto name = internal::SymbolName(elf, sections, symbols, symbol);
        if (!name.ok()) {
          return name.status();
        }
        const auto function = imports->functions.find(*name);
        if (function == imports->functions.end()) {
          return absl::UnimplementedError("Unresolved external reference");
        }
        const auto status = internal::CheckImportCall(elf, code, location, type,
                                                      function->second);
        if (!status.ok()) {
          return status;
        }
        continue;
      }
      if (index == 0 || index >= sections.size() ||
          !(sections[index].flags & 2) || address < code.address ||
          !Within(code.size, address - code.address, 1)) {
        return absl::UnimplementedError("Relocation needs an external symbol");
      }
      // R_ARM_ABS32 words in the linked ET_EXEC are already resolved.
      if (type == 2) {
        if (location % 4) {
          return absl::UnimplementedError(
              "E32 absolute pointer must be word aligned");
        }
        if (imports != nullptr && (section.info == imports->got_index ||
                                   section.info == imports->dynamic_index)) {
          return absl::UnimplementedError(
              "Absolute references in import metadata unsupported");
        }
        const uint32_t offset = location - code.address;
        const uint32_t value = Read32(elf, code.offset + offset);
        const uint8_t symbol_type = static_cast<uint8_t>(elf[sym + 12]) & 15;
        if (value < code.address ||
            !Within(code.size, value - code.address, 1) ||
            (symbol_type == 2 && ((value ^ Read32(elf, sym + 4)) & 1))) {
          return absl::DataLossError(
              "Resolved ELF absolute pointer/state outside code");
        }
        if (pointers->size() >= 65535) {
          return absl::ResourceExhaustedError("Too many E32 absolute pointers");
        }
        pointers->push_back(offset);
      }
    }
  }
  if (!retained) {
    return absl::FailedPreconditionError(
        "Retained runtime relocation records required; link with "
        "--emit-relocs");
  }
  std::sort(pointers->begin(), pointers->end());
  if (std::adjacent_find(pointers->begin(), pointers->end()) !=
      pointers->end()) {
    return absl::DataLossError("Duplicate ELF absolute pointer relocation");
  }
  return absl::OkStatus();
}

absl::StatusOr<Segment> ExtractCode(std::string_view elf,
                                    const analysis::Elf32Header& header,
                                    const std::vector<std::string>* proxies,
                                    ResolvedImports* imports,
                                    std::vector<Section>* output_sections,
                                    std::vector<uint32_t>* pointers) {
  if (header.type != 2 || header.machine != 40 || header.flags >> 24 != 5 ||
      (header.flags & 0x400)) {
    return absl::UnimplementedError("Requires ARM EABI5 soft-float ET_EXEC");
  }
  Segment code;
  bool found = false;
  for (size_t i = 0; i < header.program_count; ++i) {
    const size_t p = Read32(elf, 28) + i * Read16(elf, 42);
    const uint32_t type = Read32(elf, p);
    if (type == 2 && proxies != nullptr) {
      if (!Within(elf.size(), Read32(elf, p + 4), Read32(elf, p + 16)) ||
          Read32(elf, p + 16) != Read32(elf, p + 20) ||
          Read32(elf, p + 24) != 4) {
        return absl::DataLossError("Invalid ELF dynamic segment");
      }
      continue;
    }
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
  std::set<size_t> relro;
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
    if (section.type == 4 ||
        (proxies == nullptr && (section.type == 6 || section.type == 11))) {
      return absl::UnimplementedError("Dynamic/RELA ELF is unsupported");
    }
    if ((section.flags & 2) && section.size) {
      const auto readonly = IsRelro(elf, header, s, section);
      if (!readonly.ok()) {
        return readonly.status();
      }
      if (*readonly) {
        relro.insert(i);
      }
      if ((section.flags & 0x400) ||
          (proxies == nullptr && (section.flags & 1) && !*readonly) ||
          section.type == 8 || (section.type >= 14 && section.type <= 16)) {
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
  if (proxies != nullptr) {
    auto resolved = internal::ResolveImports(elf, sections, code, *proxies);
    if (!resolved.ok()) {
      return resolved.status();
    }
    *imports = std::move(*resolved);
    for (size_t i = 0; i < sections.size(); ++i) {
      if ((sections[i].flags & 1) && sections[i].size &&
          i != imports->got_index && i != imports->dynamic_index &&
          !relro.contains(i)) {
        return absl::UnimplementedError(
            "Writable application data unsupported");
      }
    }
    const Section& dynamic = sections[imports->dynamic_index];
    size_t dynamic_segments = 0;
    for (size_t i = 0; i < header.program_count; ++i) {
      const size_t p = Read32(elf, 28) + i * Read16(elf, 42);
      if (Read32(elf, p) == 2) {
        ++dynamic_segments;
        if (Read32(elf, p + 4) != dynamic.offset ||
            Read32(elf, p + 8) != dynamic.address ||
            Read32(elf, p + 16) != dynamic.size) {
          return absl::DataLossError("Dynamic section/segment mismatch");
        }
      }
    }
    if (dynamic_segments != 1) {
      return absl::DataLossError("Requires one ELF dynamic segment");
    }
  }
  const absl::Status status = CheckRelocations(
      elf, sections, code, proxies == nullptr ? nullptr : imports, pointers);
  if (!status.ok()) {
    return status;
  }
  *output_sections = std::move(sections);
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

absl::StatusOr<std::string> ConvertExecutable(
    std::string_view elf, uint32_t uid3,
    const std::vector<std::string>* proxies,
    const std::string_view* definition = nullptr) {
  if (elf.size() > 64 * 1024 * 1024) {
    return absl::ResourceExhaustedError("ELF exceeds 64 MiB");
  }
  if (uid3 < 0xe0000000 || uid3 > 0xefffffff) {
    return absl::InvalidArgumentError("Experimental unprotected UID3 required");
  }
  const auto header = analysis::InspectElf32(elf);
  if (!header.ok()) {
    return header.status();
  }
  ResolvedImports imports;
  std::vector<Section> sections;
  std::vector<uint32_t> relocations;
  const auto segment =
      ExtractCode(elf, *header, proxies, &imports, &sections, &relocations);
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
  std::vector<ExportSlot> exports;
  std::string bitmap;
  if (definition != nullptr) {
    auto resolved =
        internal::ResolveExports(elf, sections, *segment, entry, *definition);
    if (!resolved.ok()) {
      return resolved.status();
    }
    exports = std::move(*resolved);
    if (std::any_of(exports.begin(), exports.end(),
                    [](const ExportSlot& slot) { return slot.absent; })) {
      bitmap = internal::ExportBitmap(exports);
    }
  }
  const uint32_t header_size =
      (155 + static_cast<uint32_t>(bitmap.size()) + 3) & ~uint32_t{3};
  const uint32_t export_prefix = (segment->size + 3) & ~uint32_t{3};
  const uint32_t code_size =
      definition == nullptr
          ? segment->size
          : export_prefix + 4 + static_cast<uint32_t>(exports.size()) * 4;
  if (code_size > kMaxImageSize - header_size ||
      code_size > UINT32_MAX - segment->address) {
    return absl::ResourceExhaustedError(
        "DLL code/export layout exceeds bounds");
  }
  std::string bytes(header_size, '\0');
  Put32(bytes, 0, definition == nullptr ? 0x1000007a : 0x10000079);
  Put32(bytes, 4, definition == nullptr ? 0 : 0x1000008d);
  Put32(bytes, 8, uid3);
  Put32(bytes, 12, UidChecksum(bytes));
  Put32(bytes, 16, 0x434f5045);  // EPOC
  Put32(bytes, 24, 0x00010000);  // Module version 1.0.
  bytes[33] = 1;                 // Converter version 0.1.0.
  Put32(bytes, 44, kFlags | (definition == nullptr ? 0 : 1));
  Put32(bytes, 48, code_size);
  Put32(bytes, 56, 0x1000);
  Put32(bytes, 60, 0x100000);
  Put32(bytes, 64, 0x10000);
  Put32(bytes, 72, entry);
  Put32(bytes, 76, segment->address);
  Put32(bytes, 96, code_size);
  Put32(bytes, 100, header_size);
  Put16(bytes, 120, 350);     // Foreground process priority.
  Put16(bytes, 122, 0x2001);  // ARMv5.
  Put32(bytes, 128, uid3);    // Secure ID; no capabilities or vendor ID.
  Put16(bytes, 152, static_cast<uint16_t>(bitmap.size()));
  bytes[154] = bitmap.empty() ? 0 : 1;  // No holes or full bitmap.
  bytes.replace(155, bitmap.size(), bitmap);
  bytes.append(code);
  if (definition != nullptr) {
    if (exports.size() > 65535 - relocations.size()) {
      return absl::ResourceExhaustedError(
          "Combined E32 pointer/export relocation limit exceeded");
    }
    bytes.resize(header_size + code_size, '\0');
    Put32(bytes, 88, header_size + export_prefix + 4);
    Put32(bytes, 92, static_cast<uint32_t>(exports.size()));
    Put32(bytes, header_size + export_prefix,
          static_cast<uint32_t>(exports.size()));
    for (const auto& slot : exports) {
      const uint32_t offset = export_prefix + slot.ordinal * 4;
      Put32(bytes, header_size + offset, slot.address);
      relocations.push_back(offset);
    }
  }
  if (proxies != nullptr) {
    if (segment->size % 4) {
      return absl::DataLossError("Imported code must be word aligned");
    }
    for (const auto& block : imports.blocks) {
      for (const auto& slot : block.slots) {
        Put32(bytes, header_size + slot.code_offset, slot.ordinal);
      }
    }
    Put32(bytes, 84, static_cast<uint32_t>(imports.blocks.size()));
    Put32(bytes, 108, static_cast<uint32_t>(bytes.size()));
    bytes.append(internal::EncodeImports(imports.blocks));
  }
  if (!relocations.empty()) {
    const auto encoded = internal::EncodeCodeRelocations(relocations);
    if (!encoded.ok()) {
      return encoded.status();
    }
    Put32(bytes, 112, static_cast<uint32_t>(bytes.size()));
    bytes.append(*encoded);
  }
  Put32(bytes, 124, static_cast<uint32_t>(bytes.size() - header_size));
  Put32(bytes, 20, HeaderCrc(bytes, header_size));
  return bytes;
}

}  // namespace

absl::StatusOr<std::string> ConvertPicExecutable(std::string_view elf,
                                                 uint32_t uid3) {
  return ConvertExecutable(elf, uid3, nullptr);
}

absl::StatusOr<std::string> ConvertImportedExecutable(
    std::string_view elf, const std::vector<std::string>& proxies,
    uint32_t uid3) {
  return ConvertExecutable(elf, uid3, &proxies);
}

absl::StatusOr<std::string> ConvertDll(std::string_view elf,
                                       std::string_view definition,
                                       const std::vector<std::string>& proxies,
                                       uint32_t uid3) {
  return ConvertExecutable(elf, uid3, proxies.empty() ? nullptr : &proxies,
                           &definition);
}

absl::StatusOr<ImageInfo> InspectImage(std::string_view bytes) {
  if (bytes.size() < kHeaderSize || bytes.size() > kMaxImageSize ||
      bytes.substr(16, 4) != "EPOC") {
    return absl::DataLossError("Missing/truncated E32 V header");
  }
  const uint32_t flags = Read32(bytes, 44);
  const bool dll = flags == (kFlags | 1);
  const uint32_t header_size = Read32(bytes, 100);
  const uint16_t description_size = Read16(bytes, 152);
  const uint8_t description_type = static_cast<uint8_t>(bytes[154]);
  if ((!dll && flags != kFlags) || Read32(bytes, 28) != 0 ||
      Read16(bytes, 122) != 0x2001 || description_type > 1 ||
      (!dll && (description_size != 0 || description_type != 0))) {
    return absl::UnimplementedError("Unsupported E32 image profile");
  }
  if (header_size != ((155U + description_size + 3) & ~uint32_t{3}) ||
      header_size > 8348 || !Within(bytes.size(), 0, header_size)) {
    return absl::DataLossError("Invalid E32 extended header size");
  }
  for (size_t i = 155 + description_size; i < header_size; ++i) {
    if (bytes[i] != 0) {
      return absl::DataLossError("Nonzero E32 header padding");
    }
  }
  if (Read32(bytes, 0) != (dll ? 0x10000079U : 0x1000007aU) ||
      Read32(bytes, 4) != (dll ? 0x1000008dU : 0) ||
      Read32(bytes, 12) != UidChecksum(bytes) ||
      Read32(bytes, 20) != HeaderCrc(bytes, header_size)) {
    return absl::DataLossError("Invalid E32 identity/header checksum");
  }
  for (size_t offset :
       {size_t{52}, size_t{68}, size_t{80}, size_t{104}, size_t{116},
        size_t{132}, size_t{136}, size_t{140}, size_t{144}, size_t{148}}) {
    if (Read32(bytes, offset) != 0) {
      return absl::UnimplementedError("E32 data/capabilities unsupported");
    }
  }
  const uint32_t size = Read32(bytes, 48);
  const uint32_t uid3 = Read32(bytes, 8), base = Read32(bytes, 76);
  const uint32_t count = Read32(bytes, 84), import_offset = Read32(bytes, 108);
  const uint32_t relocation_offset = Read32(bytes, 112);
  if (!Within(bytes.size(), header_size, size) || Read32(bytes, 96) != size ||
      Read32(bytes, 124) != bytes.size() - header_size || base % 4 ||
      size > UINT32_MAX - base || uid3 < 0xe0000000 || uid3 > 0xefffffff ||
      Read32(bytes, 128) != uid3 || Read32(bytes, 24) != 0x00010000 ||
      Read32(bytes, 56) != 0x1000 || Read32(bytes, 60) != 0x100000 ||
      Read32(bytes, 64) != 0x10000) {
    return absl::DataLossError("Invalid experimental E32 layout/identity");
  }
  std::vector<ExportSlot> exports;
  std::vector<uint32_t> expected_relocations;
  uint32_t application_size = size;
  if (dll) {
    const uint32_t export_count = Read32(bytes, 92),
                   directory = Read32(bytes, 88);
    if (export_count == 0 || export_count > 65535 || size % 4 ||
        uint64_t{export_count} * 4 + 4 > size ||
        directory != header_size + size - export_count * 4 ||
        Read32(bytes, directory - 4) != export_count ||
        (description_type == 0 && description_size != 0) ||
        (description_type == 1 && description_size != (export_count + 7) / 8)) {
      return absl::DataLossError("Invalid E32 export table/bitmap layout");
    }
    application_size = size - (export_count + 1) * 4;
    for (uint32_t ordinal = 1; ordinal <= export_count; ++ordinal) {
      const uint32_t p = directory + (ordinal - 1) * 4;
      const uint32_t address = Read32(bytes, p);
      const uint32_t normalized = address & ~uint32_t{1};
      const bool absent =
          description_type == 1 &&
          !(static_cast<uint8_t>(bytes[155 + (ordinal - 1) / 8]) &
            (1U << ((ordinal - 1) % 8)));
      if ((absent && address != base + Read32(bytes, 72)) ||
          (!absent && (normalized < base ||
                       !Within(application_size, normalized - base, 1) ||
                       normalized % ((address & 1) ? 2 : 4) ||
                       address == base + Read32(bytes, 72)))) {
        return absl::DataLossError("Invalid E32 export address/absence marker");
      }
      exports.push_back({ordinal, address, absent});
      expected_relocations.push_back(p - header_size);
    }
    const bool holes =
        std::any_of(exports.begin(), exports.end(),
                    [](const ExportSlot& slot) { return slot.absent; });
    if (holes != (description_type == 1) ||
        (holes && internal::ExportBitmap(exports) !=
                      bytes.substr(155, description_size))) {
      return absl::DataLossError("Noncanonical E32 export bitmap");
    }
  } else if (Read32(bytes, 88) || Read32(bytes, 92)) {
    return absl::UnimplementedError("Executable exports unsupported");
  }
  const absl::Status status = CheckEntry(
      bytes.substr(header_size, application_size), Read32(bytes, 72));
  if (!status.ok()) {
    return status;
  }
  const size_t import_end =
      relocation_offset == 0 ? bytes.size() : relocation_offset;
  size_t consumed = header_size + size;
  std::vector<ImportBlock> imports;
  std::set<uint32_t> import_slots;
  if (count != 0) {
    if (size % 4 || import_offset != consumed || import_end < consumed ||
        import_end > bytes.size()) {
      return absl::DataLossError("Invalid E32 import section position");
    }
    auto decoded = internal::DecodeImports(
        bytes.substr(import_offset, import_end - import_offset),
        bytes.substr(header_size, application_size), count);
    if (!decoded.ok()) {
      return decoded.status();
    }
    imports = std::move(*decoded);
    for (const auto& block : imports) {
      for (const auto& slot : block.slots) {
        import_slots.insert(slot.code_offset);
      }
    }
    consumed = import_end;
  } else if (import_offset != 0) {
    return absl::DataLossError("Unexpected E32 import section");
  }
  std::vector<uint32_t> relocations;
  if (relocation_offset != 0) {
    if (relocation_offset != consumed || relocation_offset >= bytes.size()) {
      return absl::DataLossError("Missing/misplaced E32 export relocations");
    }
    auto decoded =
        internal::DecodeCodeRelocations(bytes.substr(consumed), size);
    if (!decoded.ok()) {
      return decoded.status();
    }
    relocations = std::move(*decoded);
    if (!std::includes(relocations.begin(), relocations.end(),
                       expected_relocations.begin(),
                       expected_relocations.end())) {
      return absl::DataLossError(
          "E32 export pointers must all be text relocations");
    }
    for (const uint32_t offset : relocations) {
      const uint32_t value = Read32(bytes, header_size + offset);
      if (import_slots.contains(offset) ||
          (!Within(application_size, offset, 4) &&
           !std::binary_search(expected_relocations.begin(),
                               expected_relocations.end(), offset)) ||
          value < base || !Within(application_size, value - base, 1)) {
        return absl::DataLossError(
            "E32 pointer relocation outside application code/table");
      }
    }
  } else if (dll || consumed != bytes.size()) {
    return absl::DataLossError("Unexpected E32 trailing data");
  }
  return ImageInfo{.uid3 = uid3,
                   .header_crc = Read32(bytes, 20),
                   .flags = flags,
                   .code_size = size,
                   .code_base = base,
                   .entry_offset = Read32(bytes, 72),
                   .secure_id = Read32(bytes, 128),
                   .dll = dll,
                   .header_size = header_size,
                   .imports = std::move(imports),
                   .exports = std::move(exports),
                   .code_relocations = std::move(relocations)};
}

}  // namespace symbian::e32
