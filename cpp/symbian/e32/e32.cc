#include "symbian/e32/e32.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <absl/base/nullability.h>
#include <absl/status/status.h>
#include <absl/strings/str_cat.h>

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

// Bounded per-image static storage. The linker must emit an independent,
// non-executable RW mapping; DLL lifecycle still needs execution validation.
struct DataLayout {
  Segment file;
  uint32_t bss_size = 0;

  bool Contains(uint32_t address, uint32_t size = 1) const {
    return address >= file.address &&
           Within(file.size + bss_size, address - file.address, size);
  }
};

struct Fixups {
  std::vector<uint32_t> code;
  std::set<uint32_t> code_to_data;
  std::vector<uint32_t> data;
  std::set<uint32_t> data_to_data;
};

// The original elf2e32 stores the read-only descriptor's code-relative offset
// in E32ImageHeaderV::iExceptionDescriptor, with bit zero marking presence.
// Its four words are exidx base/limit and read-only segment base/limit.
absl::StatusOr<uint32_t> ExceptionDescriptorOffset(
    std::string_view elf, const std::vector<Section>& sections,
    const Segment& code) {
  const Section* absl_nullable exidx = nullptr;
  for (const Section& section : sections) {
    if (section.type == 0x70000001 || section.name == ".ARM.exidx") {
      if (exidx != nullptr || section.name != ".ARM.exidx" ||
          section.type != 0x70000001 || !(section.flags & 2) ||
          section.address % 4 || section.size == 0 || section.size % 8 ||
          section.size > 65536 || section.address < code.address ||
          !Within(code.size, section.address - code.address, section.size)) {
        return absl::DataLossError("Invalid ARM exception index");
      }
      exidx = &section;
    }
  }
  if (exidx == nullptr) {
    return 0;
  }
  uint32_t descriptor = 0;
  for (const Section& symbols : sections) {
    if (symbols.type != 2) {
      continue;
    }
    if (symbols.entry_size != 16 || symbols.size % 16) {
      return absl::DataLossError("Invalid exception symbol table");
    }
    for (size_t symbol = 1; symbol < symbols.size / 16; ++symbol) {
      const auto name = internal::SymbolName(elf, sections, symbols, symbol);
      if (!name.ok()) {
        return name.status();
      }
      if (*name != "Symbian$$CPP$$Exception$$Descriptor") {
        continue;
      }
      const size_t p = symbols.offset + symbol * 16;
      const uint32_t address = Read32(elf, p + 4);
      const uint16_t owner = Read16(elf, p + 14);
      if (descriptor || exidx == nullptr || owner == 0 ||
          owner >= sections.size() || !(sections[owner].flags & 2) ||
          (static_cast<uint8_t>(elf[p + 12]) & 15) != 1 ||
          Read32(elf, p + 8) != 16 || address % 4 || address < code.address ||
          !Within(code.size, address - code.address, 16) ||
          address < sections[owner].address ||
          !Within(sections[owner].size, address - sections[owner].address,
                  16)) {
        return absl::DataLossError("Invalid Symbian exception descriptor");
      }
      const size_t source = code.offset + address - code.address;
      if (Read32(elf, source) != exidx->address ||
          Read32(elf, source + 4) != exidx->address + exidx->size ||
          (Read32(elf, source + 8) & ~uint32_t{1}) != code.address ||
          Read32(elf, source + 12) != code.address + code.size) {
        return absl::DataLossError("Exception descriptor bounds mismatch");
      }
      descriptor = (address - code.address) | 1;
    }
  }
  if (exidx != nullptr && !descriptor) {
    return absl::FailedPreconditionError(
        "ARM unwind index requires a Symbian exception descriptor");
  }
  return descriptor;
}

absl::StatusOr<std::string_view> SectionName(
    std::string_view elf, const analysis::Elf32Header& header,
    size_t section_header) {
  const uint16_t names_index = Read16(elf, 50);
  if (names_index == 0 || names_index >= header.section_count) {
    return std::string_view{};
  }
  const size_t names = Read32(elf, 32) + names_index * Read16(elf, 46);
  const uint32_t offset = Read32(elf, names + 16);
  const uint32_t size = Read32(elf, names + 20);
  const uint32_t name_offset = Read32(elf, section_header);
  if (Read32(elf, names + 4) != 3 || !Within(elf.size(), offset, size) ||
      name_offset >= size) {
    return absl::DataLossError("Invalid ELF section name table");
  }
  std::string_view name = elf.substr(offset + name_offset, size - name_offset);
  const size_t end = name.find('\0');
  if (end == std::string_view::npos) {
    return absl::DataLossError("Unterminated ELF section name");
  }
  return name.substr(0, end);
}

absl::Status CheckRelocations(std::string_view elf,
                              const std::vector<Section>& sections,
                              const Segment& code, const DataLayout& data,
                              const ResolvedImports* absl_nullable imports,
                              size_t local_got_index,
                              Fixups* absl_nonnull pointers) {
  bool retained = false;
  std::set<uint32_t> got_symbols;
  std::set<uint32_t> checked_import_pointers;
  for (size_t section_index = 0; section_index < sections.size();
       ++section_index) {
    const Section& section = sections[section_index];
    if (imports != nullptr &&
        (section_index == imports->relocation_index ||
         section_index == imports->data_relocation_index)) {
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
      // GOT_PREL references remain PC-relative; only their GOT words need
      // loader fixups. MOVW/MOVT and other dynamic contracts stay unsupported.
      if (type != 2 && type != 1 && type != 3 && type != 10 && type != 28 &&
          type != 29 && type != 30 && type != 38 && type != 41 && type != 42 &&
          type != 96) {
        return absl::UnimplementedError("Unsupported ARM relocation type");
      }
      if (type == 38 && target.name != ".init_array" &&
          target.name != ".fini_array") {
        return absl::UnimplementedError(
            "R_ARM_TARGET1 is supported only in C++ lifecycle arrays");
      }
      if (location < target.address ||
          !Within(target.size, location - target.address, 4) ||
          symbol >= symbols.size / 16) {
        return absl::DataLossError("ELF relocation is outside its target");
      }
      const size_t sym = symbols.offset + static_cast<size_t>(symbol) * 16;
      const uint16_t index = Read16(elf, sym + 14);
      const uint8_t symbol_type = static_cast<uint8_t>(elf[sym + 12]) & 15;
      const uint32_t value_address = Read32(elf, sym + 4);
      const uint32_t address =
          symbol_type == 2 ? value_address & ~uint32_t{1} : value_address;
      bool code_limit = false;
      if (type == 2 && target.name == ".rodata" &&
          address == uint64_t{code.address} + code.size) {
        const auto name = internal::SymbolName(elf, sections, symbols, symbol);
        if (!name.ok()) {
          return name.status();
        }
        code_limit = *name == "__ro_end";
      }
      const bool symbol_in_data = data.Contains(address);
      const bool target_in_data = data.Contains(location, 4);
      if (index == 0 && imports != nullptr) {
        const auto name = internal::SymbolName(elf, sections, symbols, symbol);
        if (!name.ok()) {
          return name.status();
        }
        const auto object = imports->objects.find(*name);
        if (object != imports->objects.end()) {
          const auto pointer = imports->code_object_pointers.find(location);
          if (type == 2 && pointer != imports->code_object_pointers.end()) {
            if (symbol_type != 1 || target_in_data || target.type == 8 ||
                location % 4 ||
                Read32(elf, target.offset + location - target.address) !=
                    pointer->second) {
              return absl::DataLossError(
                  "Invalid retained imported-object pointer");
            }
            continue;
          }
          if (symbol_type != 1 || (type != 96 && type != 41) ||
              target_in_data || location % 4) {
            return absl::UnimplementedError(
                "Imported object requires a code GOT_PREL reference");
          }
          if (type == 41 && (target.name != ".ARM.extab" ||
                             location + Read32(elf, target.offset + location -
                                                        target.address) !=
                                 code.address + object->second)) {
            return absl::DataLossError(
                "EHABI TARGET2 references the wrong imported GOT slot");
          }
          continue;  // The loader resolves the validated GOT slot by ordinal.
        }
        const auto function = imports->functions.find(*name);
        if (function == imports->functions.end()) {
          return absl::UnimplementedError("Unresolved external reference");
        }
        if (type == 2) {
          const auto code_pointer =
              imports->code_function_pointers.find(location);
          if (code_pointer != imports->code_function_pointers.end()) {
            if (symbol_type != 2 || code_pointer->second != *name ||
                (target.name != ".rodata" && target.name != ".text") ||
                target_in_data ||
                Read32(elf, target.offset + location - target.address) != 0) {
              return absl::DataLossError(
                  "Invalid retained imported-function code pointer");
            }
            continue;
          }
          const auto data_pointer =
              imports->data_function_pointers.find(location);
          if (data_pointer != imports->data_function_pointers.end()) {
            const auto plt = imports->plt_functions.find(*name);
            if (plt == imports->plt_functions.end() ||
                plt->second != data_pointer->second || !target_in_data ||
                target.type == 8 || location % 4 ||
                Read32(elf, target.offset + location - target.address) != 0 ||
                !checked_import_pointers.insert(location).second) {
              return absl::DataLossError(
                  "Invalid retained imported-function data pointer");
            }
            continue;
          }
          const auto plt = imports->plt_functions.find(*name);
          if (plt == imports->plt_functions.end() ||
              value_address != plt->second || location % 4 ||
              (section.info == imports->got_index ||
               section.info == imports->dynamic_index) ||
              target.type == 8 ||
              (!target_in_data &&
               (location < code.address ||
                !Within(code.size, location - code.address, 4))) ||
              Read32(elf, target.offset + location - target.address) !=
                  plt->second) {
            return absl::UnimplementedError(
                "Imported function pointer requires its validated PLT slot");
          }
          auto& offsets = target_in_data ? pointers->data : pointers->code;
          if (offsets.size() >= 65535) {
            return absl::ResourceExhaustedError(
                "Too many E32 absolute pointers");
          }
          offsets.push_back(
              location - (target_in_data ? data.file.address : code.address));
          continue;
        }
        const auto status = internal::CheckImportCall(elf, code, location, type,
                                                      function->second);
        if (!status.ok()) {
          return status;
        }
        continue;
      }
      if (index == 0 || index >= sections.size() ||
          !(sections[index].flags & 2) ||
          (!symbol_in_data && !code_limit &&
           (address < code.address ||
            !Within(code.size, address - code.address, 1)))) {
        return absl::UnimplementedError(
            absl::StrCat("Relocation needs an external symbol: type=", type,
                         " target=", target.name, " location=", location));
      }
      if (symbol_type == 2 && symbol_in_data) {
        return absl::DataLossError("Function symbol in non-executable data");
      }
      if ((type == 96 || type == 41) && target_in_data) {
        return absl::UnimplementedError("GOT_PREL source must be in code");
      }
      if (symbol_in_data != target_in_data && type != 2 && type != 38 &&
          type != 96 && type != 41) {
        return absl::UnimplementedError(
            "PC-relative reference crosses independently relocated code/data "
            "mappings");
      }
      if (local_got_index != 0 && section.info == local_got_index) {
        return absl::UnimplementedError(
            "Retained relocations targeting the local GOT unsupported");
      }
      if (type == 96 || type == 41) {  // GOT(S) + A - P, already linked.
        if (local_got_index == 0 || location % 4) {
          return absl::DataLossError(
              "Missing local GOT or unaligned reference");
        }
        const uint32_t value = Read32(elf, sym + 4);
        if (type == 41) {
          // Clang EHABI uses TARGET2 with LLD's GOT-relative semantics.
          // Other TARGET2 modes and uses remain unsupported.
          const Section& got = sections[local_got_index];
          const uint32_t slot =
              location + Read32(elf, target.offset + location - target.address);
          if (target.name != ".ARM.extab" || slot % 4 || slot < got.address ||
              !Within(got.size, slot - got.address, 4) ||
              Read32(elf, got.offset + slot - got.address) != value) {
            return absl::UnimplementedError(
                "TARGET2 requires an EHABI local GOT reference");
          }
        }
        const Section& owner = sections[index];
        const uint32_t target_address = symbol_type == 2 ? address : value;
        bool lifecycle_boundary = false;
        if (symbol_type == 0 && Read32(elf, sym + 8) == 0 &&
            (owner.name == ".init_array" || owner.name == ".fini_array")) {
          const auto name =
              internal::SymbolName(elf, sections, symbols, symbol);
          if (!name.ok()) {
            return name.status();
          }
          lifecycle_boundary =
              (*name == "__init_array_start" && owner.name == ".init_array" &&
               target_address == owner.address) ||
              (*name == "__init_array_end" && owner.name == ".init_array" &&
               target_address == owner.address + owner.size) ||
              (*name == "__fini_array_start" && owner.name == ".fini_array" &&
               target_address == owner.address) ||
              (*name == "__fini_array_end" && owner.name == ".fini_array" &&
               target_address == owner.address + owner.size);
        }
        if ((symbol_type != 1 && symbol_type != 2 && !lifecycle_boundary) ||
            index == local_got_index || target_address < owner.address ||
            (!Within(owner.size, target_address - owner.address, 1) &&
             !lifecycle_boundary)) {
          return absl::UnimplementedError(
              "Local GOT requires a defined object/function in its section");
        }
        got_symbols.insert(value);  // Preserve Thumb state.
      }
      // R_ARM_ABS32 words in the linked ET_EXEC are already resolved.
      if (type == 2 || type == 38) {
        if (location % 4) {
          return absl::UnimplementedError(
              "E32 absolute pointer must be word aligned");
        }
        if (imports != nullptr && (section.info == imports->got_index ||
                                   section.info == imports->dynamic_index)) {
          return absl::UnimplementedError(
              "Absolute references in import metadata unsupported");
        }
        if (target.type == 8) {
          return absl::DataLossError("Relocation source is uninitialized BSS");
        }
        const uint32_t value =
            Read32(elf, target.offset + location - target.address);
        const bool value_in_data = data.Contains(value);
        if ((!value_in_data && !code_limit &&
             (value < code.address ||
              !Within(code.size, value - code.address, 1))) ||
            value_in_data != symbol_in_data ||
            (code_limit && value != code.address + code.size) ||
            (symbol_type == 2 && ((value ^ Read32(elf, sym + 4)) & 1))) {
          return absl::DataLossError(
              "Resolved ELF absolute pointer/state outside its mapping");
        }
        auto& offsets = target_in_data ? pointers->data : pointers->code;
        const uint32_t offset =
            location - (target_in_data ? data.file.address : code.address);
        if (offsets.size() >= 65535) {
          return absl::ResourceExhaustedError("Too many E32 absolute pointers");
        }
        offsets.push_back(offset);
        if (value_in_data) {
          (target_in_data ? pointers->data_to_data : pointers->code_to_data)
              .insert(offset);
        }
      }
    }
  }
  if (!retained) {
    return absl::FailedPreconditionError(
        "Retained runtime relocation records required; link with "
        "--emit-relocs");
  }
  // LLD may create an ARM-to-Thumb long thunk after input relocations have
  // been retained. Its absolute target word has no R_ARM_ABS32 record, so
  // recognize the exact generated symbol/instruction and relocate that word.
  for (const Section& symbols : sections) {
    if (symbols.type != 2 || symbols.entry_size != 16 || symbols.size % 16) {
      continue;
    }
    for (size_t symbol = 1; symbol < symbols.size / 16; ++symbol) {
      const size_t entry = symbols.offset + symbol * 16;
      const uint32_t address = Read32(elf, entry + 4);
      if (elf[entry + 12] != 2 || Read32(elf, entry + 8) != 8 || address % 4 ||
          address < code.address ||
          !Within(code.size, address - code.address, 8) ||
          Read32(elf, code.offset + address - code.address) != 0xe51ff004) {
        continue;
      }
      const auto name = internal::SymbolName(elf, sections, symbols, symbol);
      if (!name.ok()) {
        return name.status();
      }
      if (!name->starts_with("__ARMv5LongLdrPcThunk_")) {
        continue;
      }
      const uint16_t owner_index = Read16(elf, entry + 14);
      if (owner_index == 0 || owner_index >= sections.size() || address % 4 ||
          address < code.address ||
          !Within(code.size, address - code.address, 8)) {
        return absl::DataLossError("Invalid generated ARM interworking thunk");
      }
      const Section& owner = sections[owner_index];
      if (owner.type != 1 || owner.flags != 6 || address < owner.address ||
          !Within(owner.size, address - owner.address, 8)) {
        return absl::DataLossError(
            "Interworking thunk outside executable text");
      }
      const size_t file = code.offset + address - code.address;
      const uint32_t target = Read32(elf, file + 4) & ~uint32_t{1};
      if (Read32(elf, file) != 0xe51ff004 || target < code.address ||
          !Within(code.size, target - code.address, 1)) {
        return absl::DataLossError("Unsupported ARM interworking thunk target");
      }
      const uint32_t offset = address + 4 - code.address;
      if (std::find(pointers->code.begin(), pointers->code.end(), offset) ==
          pointers->code.end()) {
        pointers->code.push_back(offset);
      }
    }
  }
  if (imports != nullptr) {
    if (checked_import_pointers.size() !=
        imports->data_function_pointers.size()) {
      return absl::DataLossError(
          "Imported-function data pointer lacks retained relocation");
    }
    for (const auto& [location, value] : imports->data_function_pointers) {
      if (!data.Contains(location, 4) ||
          !Within(data.file.size, location - data.file.address, 4) ||
          Read32(elf, data.file.offset + location - data.file.address) != 0) {
        return absl::DataLossError(
            "Imported-function pointer outside initialized data");
      }
      pointers->data.push_back(location - data.file.address);
    }
  }
  if (local_got_index != 0) {
    const Section& got = sections[local_got_index];
    std::set<uint32_t> resolved;
    for (uint32_t i = 0; i < got.size; i += 4) {
      const uint32_t slot = got.address - code.address + i;
      if (imports != nullptr && imports->object_slots.contains(slot)) {
        continue;  // Eager imported object, not a local pointer relocation.
      }
      const uint32_t value = Read32(elf, got.offset + i);
      if (!got_symbols.contains(value)) {
        return absl::DataLossError("Local GOT word lacks a retained symbol");
      }
      resolved.insert(value);
      const uint32_t offset = got.address - code.address + i;
      pointers->code.push_back(offset);
      if (data.Contains(value)) {
        pointers->code_to_data.insert(offset);
      }
    }
    if (resolved != got_symbols) {
      return absl::DataLossError("Retained GOT symbol lacks a local GOT word");
    }
  }
  for (auto* absl_nonnull offsets : {&pointers->code, &pointers->data}) {
    if (offsets->size() > 65535) {
      return absl::ResourceExhaustedError("Too many E32 absolute pointers");
    }
    std::sort(offsets->begin(), offsets->end());
    if (std::adjacent_find(offsets->begin(), offsets->end()) !=
        offsets->end()) {
      return absl::DataLossError("Duplicate ELF absolute pointer relocation");
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<Segment> ExtractCode(
    std::string_view elf, const analysis::Elf32Header& header,
    const std::vector<std::string>* absl_nullable proxies,
    ResolvedImports* absl_nonnull imports,
    std::vector<Section>* absl_nonnull output_sections,
    Fixups* absl_nonnull pointers, DataLayout* absl_nullable data) {
  if (header.type != 2 || header.machine != 40 || header.flags >> 24 != 5 ||
      (header.flags & 0x400)) {
    return absl::UnimplementedError("Requires ARM EABI5 soft-float ET_EXEC");
  }
  if ((header.arm.cpu_arch && header.arm.cpu_arch != 3 &&
       header.arm.cpu_arch != 6) ||
      header.arm.thumb_isa > 1 || header.arm.fp_arch || header.arm.simd_arch ||
      header.arm.vfp_args) {
    return absl::UnimplementedError(
        "SDK supports ARMv5T/ARMv6, Thumb-1, soft-float only; rebuild with "
        "SYMBIAN_TARGET_ARCH=armv6 or armv5t and -mfloat-abi=soft");
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
    if (type == 1 && Read32(elf, p + 24) == 6) {
      const Segment file{Read32(elf, p + 4), Read32(elf, p + 8),
                         Read32(elf, p + 16)};
      const uint32_t memory = Read32(elf, p + 20),
                     alignment = Read32(elf, p + 28);
      if (memory == 0 && file.size == 0) {
        continue;
      }
      if (data->file.size || data->bss_size || memory < file.size ||
          memory > 1024 * 1024 || !file.address || file.address % 4 ||
          file.size % 4 || memory > UINT32_MAX - file.address ||
          !Within(elf.size(), file.offset, file.size) ||
          (alignment > 1 &&
           ((alignment & (alignment - 1)) ||
            file.offset % alignment != file.address % alignment))) {
        return absl::DataLossError("Invalid/duplicate bounded RW data segment");
      }
      *data = {file, memory - file.size};
      continue;
    }
    if (type != 1 || found || Read32(elf, p + 24) != 5 ||
        Read32(elf, p + 16) != Read32(elf, p + 20)) {
      return absl::UnimplementedError(
          "Requires one RX and at most one non-executable RW load segment");
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
  if ((data->file.size || data->bss_size) &&
      !(uint64_t{code.address} + code.size <= data->file.address ||
        uint64_t{data->file.address} + data->file.size + data->bss_size <=
            code.address)) {
    return absl::DataLossError("Overlapping ELF code/data mappings");
  }
  if (data->file.size &&
      !(uint64_t{code.offset} + code.size <= data->file.offset ||
        uint64_t{data->file.offset} + data->file.size <= code.offset)) {
    return absl::DataLossError("Overlapping ELF code/data file ranges");
  }
  std::vector<Section> sections;
  std::set<size_t> relro;
  std::vector<size_t> lifecycle_arrays;
  size_t local_got_index = 0;
  for (size_t i = 0; i < header.section_count; ++i) {
    const size_t s = Read32(elf, 32) + i * Read16(elf, 46);
    Section section{Read32(elf, s + 4),  Read32(elf, s + 8),
                    Read32(elf, s + 12), Read32(elf, s + 16),
                    Read32(elf, s + 20), Read32(elf, s + 24),
                    Read32(elf, s + 28), Read32(elf, s + 36)};
    if (section.flags & 2) {
      const auto name = SectionName(elf, header, s);
      if (!name.ok()) {
        return name.status();
      }
      section.name = *name;
    }
    if (section.type != 8 &&
        !Within(elf.size(), section.offset, section.size)) {
      return absl::DataLossError("ELF section outside file");
    }
    if (section.type == 4 ||
        (proxies == nullptr && (section.type == 6 || section.type == 11))) {
      return absl::UnimplementedError("Dynamic/RELA ELF is unsupported");
    }
    if ((section.flags & 2) && section.size) {
      // These tables are writable only during loader fixups, then live in RX.
      const bool readonly = section.type == 1 && (section.flags & 5) == 1 &&
                            (section.name == ".data.rel.ro" ||
                             section.name.starts_with(".data.rel.ro."));
      const bool local_got = section.name == ".got";
      const bool lifecycle_array =
          (section.name == ".init_array" && section.type == 14) ||
          (section.name == ".fini_array" && section.type == 15);
      if (lifecycle_array) {
        if (section.flags != 3 || section.address % 4 || section.size % 4 ||
            section.size > 1024 ||
            data->Contains(section.address, section.size)) {
          return absl::DataLossError("Invalid bounded C++ lifecycle array");
        }
        lifecycle_arrays.push_back(i);
      }
      if (local_got) {
        if (local_got_index != 0 || section.type != 1 || section.flags != 3 ||
            section.address % 4 || section.size % 4 || section.size > 4096) {
          return absl::DataLossError("Invalid/duplicate bounded local GOT");
        }
        local_got_index = i;
      }
      if (readonly) {
        relro.insert(i);
      }
      const bool writable = data->Contains(section.address, section.size);
      if (local_got && writable) {
        return absl::UnimplementedError(
            "Local GOT must be in the code mapping");
      }
      if ((section.flags & 0x400) ||
          (proxies == nullptr && (section.flags & 1) && !readonly &&
           !local_got && !lifecycle_array && !writable) ||
          (!writable && section.type == 8) ||
          ((section.type >= 14 && section.type <= 16) && !lifecycle_array)) {
        return absl::UnimplementedError("Data/TLS/constructors unsupported");
      }
      if (writable) {
        const auto& file = data->file;
        if (section.flags != 3 || (section.type != 1 && section.type != 8) ||
            (section.type == 8 && section.address < file.address + file.size) ||
            (section.type == 1 &&
             (!Within(file.size, section.address - file.address,
                      section.size) ||
              section.offset < file.offset ||
              section.offset - file.offset !=
                  section.address - file.address))) {
          return absl::DataLossError(
              "Writable section outside data/BSS layout");
        }
        sections.push_back(section);
        continue;
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
          i != local_got_index && !relro.contains(i) &&
          std::find(lifecycle_arrays.begin(), lifecycle_arrays.end(), i) ==
              lifecycle_arrays.end() &&
          !data->Contains(sections[i].address, sections[i].size)) {
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
      elf, sections, code, *data, proxies == nullptr ? nullptr : imports,
      local_got_index, pointers);
  if (!status.ok()) {
    return status;
  }
  for (size_t index : lifecycle_arrays) {
    const Section& array = sections[index];
    for (uint32_t i = 0; i < array.size; i += 4) {
      const uint32_t offset = array.address - code.address + i;
      if (!std::binary_search(pointers->code.begin(), pointers->code.end(),
                              offset) ||
          pointers->code_to_data.contains(offset)) {
        return absl::DataLossError(
            "C++ lifecycle entry lacks a code relocation");
      }
    }
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
    std::string_view elf, uint32_t uid3, uint32_t capabilities,
    const std::vector<std::string>* absl_nullable proxies,
    const std::string_view* absl_nullable definition = nullptr) {
  if (elf.size() > 64 * 1024 * 1024) {
    return absl::ResourceExhaustedError("ELF exceeds 64 MiB");
  }
  if (uid3 < 0xe0000000 || uid3 > 0xefffffff) {
    return absl::InvalidArgumentError("Experimental unprotected UID3 required");
  }
  constexpr uint32_t kNetworkServices = 1U << 13;
  if ((capabilities & ~kNetworkServices) != 0) {
    return absl::UnimplementedError("Unsupported E32 capability");
  }
  const auto header = analysis::InspectElf32(elf);
  if (!header.ok()) {
    return header.status();
  }
  ResolvedImports imports;
  std::vector<Section> sections;
  Fixups fixups;
  DataLayout data;
  auto& relocations = fixups.code;
  const auto segment =
      ExtractCode(elf, *header, proxies, &imports, &sections, &fixups, &data);
  if (!segment.ok()) {
    return segment.status();
  }
  const auto exception_descriptor =
      ExceptionDescriptorOffset(elf, sections, *segment);
  if (!exception_descriptor.ok()) {
    return exception_descriptor.status();
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
  Put32(bytes, 52, data.file.size);
  Put32(bytes, 68, data.bss_size);
  Put32(bytes, 80, data.file.address);
  Put32(bytes, 56, 0x1000);
  Put32(bytes, 60, 0x100000);
  Put32(bytes, 64, 0x10000);
  Put32(bytes, 72, entry);
  Put32(bytes, 76, segment->address);
  Put32(bytes, 96, code_size);
  Put32(bytes, 100, header_size);
  Put16(bytes, 120, 350);  // Foreground process priority.
  Put16(bytes, 122, header->arm.cpu_arch == 6 ? 0x2002 : 0x2001);
  Put32(bytes, 128, uid3);  // Secure ID; no vendor ID.
  Put32(bytes, 136, capabilities);
  Put32(bytes, 144, *exception_descriptor);
  Put16(bytes, 152, static_cast<uint16_t>(bitmap.size()));
  bytes[154] = bitmap.empty() ? 0 : 1;  // No holes or full bitmap.
  bytes.replace(155, bitmap.size(), bitmap);
  bytes.append(code);
  // The publisher extends the RX mapping with the frozen export directory.
  // Keep the validated EHABI descriptor's read-only limit in that mapping.
  if (definition != nullptr && *exception_descriptor != 0) {
    Put32(bytes, header_size + (*exception_descriptor & ~uint32_t{1}) + 12,
          segment->address + code_size);
  }
  if (definition != nullptr) {
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
  if (data.file.size) {
    Put32(bytes, 104, static_cast<uint32_t>(bytes.size()));
    bytes.append(elf.substr(data.file.offset, data.file.size));
    for (const auto& [location, plt] : imports.data_function_pointers) {
      Put32(bytes, Read32(bytes, 104) + location - data.file.address, plt);
    }
  }
  if (proxies != nullptr) {
    if (segment->size % 4) {
      return absl::DataLossError("Imported code must be word aligned");
    }
    for (const auto& block : imports.blocks) {
      for (const auto& slot : block.slots) {
        Put32(bytes, header_size + slot.code_offset,
              slot.ordinal | (slot.addend << 16));
      }
    }
    Put32(bytes, 84, static_cast<uint32_t>(imports.blocks.size()));
    Put32(bytes, 108, static_cast<uint32_t>(bytes.size()));
    bytes.append(internal::EncodeImports(imports.blocks));
  }
  if (!relocations.empty()) {
    const auto encoded =
        internal::EncodeCodeRelocations(relocations, fixups.code_to_data);
    if (!encoded.ok()) {
      return encoded.status();
    }
    Put32(bytes, 112, static_cast<uint32_t>(bytes.size()));
    bytes.append(*encoded);
  }
  if (!fixups.data.empty()) {
    auto encoded =
        internal::EncodeCodeRelocations(fixups.data, fixups.data_to_data);
    if (!encoded.ok()) {
      return encoded.status();
    }
    Put32(bytes, 116, static_cast<uint32_t>(bytes.size()));
    bytes.append(*encoded);
  }
  Put32(bytes, 124, static_cast<uint32_t>(bytes.size() - header_size));
  Put32(bytes, 20, HeaderCrc(bytes, header_size));
  return bytes;
}

constexpr uint32_t kEka1HeaderSize = 124;

uint32_t CodeChecksum(std::string_view code) {
  uint32_t checksum = 0;
  for (size_t i = 0; i < code.size(); i += 4) {
    checksum += Read32(code, i);
  }
  return checksum;
}

absl::StatusOr<ImageInfo> InspectEka1Image(std::string_view bytes) {
  if (bytes.size() < kEka1HeaderSize || bytes.size() > kMaxImageSize) {
    return absl::DataLossError("Truncated EKA1 header");
  }
  if (Read32(bytes, 20) != 0x2000 || Read32(bytes, 44) != 0) {
    return absl::UnimplementedError("Unsupported EKA1 CPU/header profile");
  }
  // Exports, data and relocation sections remain outside this profile.
  for (size_t offset :
       {size_t{28}, size_t{52}, size_t{68}, size_t{80}, size_t{88}, size_t{92},
        size_t{104}, size_t{112}, size_t{116}}) {
    if (Read32(bytes, offset) != 0) {
      return absl::UnimplementedError("EKA1 probe requires read-only PIC code");
    }
  }
  const uint32_t size = Read32(bytes, 48), base = Read32(bytes, 76);
  const uint32_t entry = Read32(bytes, 72), uid3 = Read32(bytes, 8);
  const uint32_t count = Read32(bytes, 84), text_size = Read32(bytes, 96);
  const uint32_t import_offset = Read32(bytes, 108);
  if (Read32(bytes, 0) != 0x1000007a || Read32(bytes, 4) != 0 ||
      uid3 < 0xe0000000 || uid3 > 0xefffffff ||
      Read32(bytes, 12) != UidChecksum(bytes) ||
      Read32(bytes, 100) != kEka1HeaderSize || size < 16 || size % 4 ||
      !Within(bytes.size(), kEka1HeaderSize, size) || text_size < 16 ||
      text_size > size || text_size % 4 || base % 4 ||
      size > UINT32_MAX - base || entry % 4 || !Within(text_size, entry, 4) ||
      Read32(bytes, 56) != 0x1000 || Read32(bytes, 60) != 0x100000 ||
      Read32(bytes, 64) != 0x10000 || Read32(bytes, 120) != 350 ||
      Read32(bytes, 24) != CodeChecksum(bytes.substr(kEka1HeaderSize, size))) {
    return absl::DataLossError("Invalid EKA1 probe identity/layout/checksum");
  }
  std::vector<ImportBlock> imports;
  if (count == 0) {
    if (import_offset != 0 || text_size != size ||
        bytes.size() != kEka1HeaderSize + size) {
      return absl::DataLossError("Unexpected EKA1 trailing/import bytes");
    }
  } else {
    if (count != 1 || import_offset != kEka1HeaderSize + size) {
      return absl::UnimplementedError("EKA1 supports one final EUSER IAT");
    }
    auto decoded = internal::DecodePeImports(
        bytes.substr(import_offset), bytes.substr(kEka1HeaderSize, size),
        text_size, count);
    if (!decoded.ok()) {
      return decoded.status();
    }
    if (decoded->front().dll != "euser.dll") {
      return absl::UnimplementedError("EKA1 import profile requires EUSER");
    }
    imports = std::move(*decoded);
  }
  ImageInfo info;
  info.kernel = "eka1";
  info.uid3 = uid3;
  info.architecture = "armv5t";
  info.code_size = size;
  info.code_base = base;
  info.entry_offset = entry;
  info.header_size = kEka1HeaderSize;
  info.imports = std::move(imports);
  return info;
}

}  // namespace

absl::StatusOr<std::string> ConvertPicExecutable(std::string_view elf,
                                                 uint32_t uid3,
                                                 uint32_t capabilities) {
  return ConvertExecutable(elf, uid3, capabilities, nullptr);
}

absl::StatusOr<std::string> ConvertEka1Executable(
    std::string_view elf, uint32_t uid3,
    const std::vector<std::string>& proxies) {
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
  if (header->arm.cpu_arch != 3) {
    return absl::UnimplementedError("EKA1 probe requires ARMv5T input");
  }
  ResolvedImports imports;
  std::vector<Section> sections;
  Fixups fixups;
  DataLayout data;
  const auto segment =
      ExtractCode(elf, *header, proxies.empty() ? nullptr : &proxies, &imports,
                  &sections, &fixups, &data);
  if (!segment.ok()) {
    return segment.status();
  }
  if (data.file.size || data.bss_size || !fixups.code.empty() ||
      !fixups.data.empty()) {
    return absl::UnimplementedError("EKA1 probe requires read-only PIC code");
  }
  for (const auto& section : sections) {
    if ((section.flags & 2) && section.size &&
        (section.type == 14 || section.type == 15 || section.type == 16 ||
         section.type == 0x70000001 || section.name == ".ARM.exidx" ||
         section.name == ".ARM.extab")) {
      return absl::UnimplementedError("EKA1 lifecycle/unwinding unsupported");
    }
  }
  if (header->entry < segment->address || header->entry % 4 ||
      !Within(segment->size, header->entry - segment->address, 4) ||
      segment->size % 4) {
    return absl::DataLossError("EKA1 requires a word-aligned ARM entry/code");
  }
  uint32_t text_size = segment->size;
  std::string code(elf.substr(segment->offset, segment->size));
  if (!proxies.empty()) {
    if (imports.blocks.size() != 1 ||
        imports.blocks.front().dll != "euser.dll" ||
        !imports.data_function_pointers.empty()) {
      return absl::UnimplementedError(
          "EKA1 supports one function-only EUSER proxy");
    }
    const auto& got = sections[imports.got_index];
    text_size = got.address - segment->address + 12;
    if (got.address - segment->address + got.size != segment->size) {
      return absl::FailedPreconditionError(
          "EKA1 .got.plt must end the RX mapping");
    }
    uint32_t location = text_size;
    for (const auto& slot : imports.blocks.front().slots) {
      if (slot.code_offset != location) {
        return absl::DataLossError("EKA1 imports require a contiguous IAT");
      }
      Put32(code, location, slot.ordinal | (slot.addend << 16));
      location += 4;
    }
    code.append(4, '\0');
  }
  if (code.size() > UINT32_MAX - segment->address ||
      code.size() > kMaxImageSize - kEka1HeaderSize ||
      !Within(text_size, header->entry - segment->address, 4)) {
    return absl::DataLossError("EKA1 IAT/code bounds or entry invalid");
  }
  std::string bytes(kEka1HeaderSize, '\0');
  Put32(bytes, 0, 0x1000007a);
  Put32(bytes, 8, uid3);
  Put32(bytes, 12, UidChecksum(bytes));
  Put32(bytes, 16, 0x434f5045);
  Put32(bytes, 20, 0x2000);  // Legacy ARM CPU, not a header CRC.
  Put32(bytes, 24, CodeChecksum(code));
  bytes[33] = 1;
  Put32(bytes, 48, static_cast<uint32_t>(code.size()));
  Put32(bytes, 56, 0x1000);
  Put32(bytes, 60, 0x100000);
  Put32(bytes, 64, 0x10000);
  Put32(bytes, 72, header->entry - segment->address);
  Put32(bytes, 76, segment->address);
  Put32(bytes, 96, text_size);
  Put32(bytes, 100, kEka1HeaderSize);
  Put32(bytes, 120, 350);
  bytes.append(code);
  if (!imports.blocks.empty()) {
    Put32(bytes, 84, 1);
    Put32(bytes, 108, static_cast<uint32_t>(bytes.size()));
    bytes.append(internal::EncodePeImports(imports.blocks));
  }
  return bytes;
}

absl::StatusOr<std::string> ConvertImportedExecutable(
    std::string_view elf, const std::vector<std::string>& proxies,
    uint32_t uid3, uint32_t capabilities) {
  return ConvertExecutable(elf, uid3, capabilities, &proxies);
}

absl::StatusOr<std::string> ConvertDll(std::string_view elf,
                                       std::string_view definition,
                                       const std::vector<std::string>& proxies,
                                       uint32_t uid3, uint32_t capabilities) {
  return ConvertExecutable(elf, uid3, capabilities,
                           proxies.empty() ? nullptr : &proxies, &definition);
}

absl::StatusOr<ImageInfo> InspectImage(std::string_view bytes) {
  if (bytes.size() >= 24 && bytes.substr(16, 4) == "EPOC" &&
      Read32(bytes, 20) == 0x2000) {
    return InspectEka1Image(bytes);
  }
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
      (Read16(bytes, 122) != 0x2001 && Read16(bytes, 122) != 0x2002) ||
      description_type > 1 ||
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
  for (size_t offset : {size_t{132}, size_t{140}, size_t{148}}) {
    if (Read32(bytes, offset) != 0) {
      return absl::UnimplementedError("Unsupported E32 security header");
    }
  }
  constexpr uint32_t kNetworkServices = 1U << 13;
  if ((Read32(bytes, 136) & ~kNetworkServices) != 0) {
    return absl::UnimplementedError("Unsupported E32 capability");
  }
  const uint32_t descriptor = Read32(bytes, 144);
  if (descriptor != 0) {
    const uint32_t offset = descriptor & ~uint32_t{1};
    const uint32_t size = Read32(bytes, 48);
    const uint32_t base = Read32(bytes, 76);
    if (!(descriptor & 1) || offset % 4 || uint64_t{base} + size > UINT32_MAX ||
        !Within(size, offset, 16) ||
        !Within(bytes.size(), header_size + offset, 16)) {
      return absl::DataLossError("Invalid E32 exception descriptor offset");
    }
    const size_t p = header_size + offset;
    const uint32_t exidx_base = Read32(bytes, p);
    const uint32_t exidx_limit = Read32(bytes, p + 4);
    if ((Read32(bytes, p + 8) & ~uint32_t{1}) != base ||
        Read32(bytes, p + 12) != base + size || exidx_base < base ||
        exidx_base % 4 || exidx_limit <= exidx_base ||
        (exidx_limit - exidx_base) % 8 || exidx_limit > base + size) {
      return absl::DataLossError("Invalid E32 exception descriptor bounds");
    }
  }
  const uint32_t size = Read32(bytes, 48);
  const uint32_t uid3 = Read32(bytes, 8), base = Read32(bytes, 76);
  const uint32_t count = Read32(bytes, 84), import_offset = Read32(bytes, 108);
  const uint32_t relocation_offset = Read32(bytes, 112);
  const uint32_t data_relocation_offset = Read32(bytes, 116);
  const uint32_t data_size = Read32(bytes, 52), bss_size = Read32(bytes, 68),
                 data_base = Read32(bytes, 80),
                 data_offset = Read32(bytes, 104);
  if (uint64_t{data_size} + bss_size > 1024 * 1024 || data_size % 4 ||
      (data_size != 0 && (data_offset != header_size + size ||
                          !Within(bytes.size(), data_offset, data_size))) ||
      (data_size == 0 && data_offset != 0) || data_base % 4 ||
      (data_size == 0 && bss_size == 0 &&
       (data_base || data_relocation_offset)) ||
      ((data_size || bss_size) &&
       (!data_base || uint64_t{data_base} + data_size + bss_size > UINT32_MAX ||
        !(uint64_t{base} + size <= data_base ||
          uint64_t{data_base} + data_size + bss_size <= base)))) {
    return absl::DataLossError("Invalid bounded E32 data/BSS layout");
  }

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
      relocation_offset
          ? relocation_offset
          : (data_relocation_offset ? data_relocation_offset : bytes.size());
  size_t consumed = header_size + size + data_size;
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
  std::vector<uint32_t> relocations, data_relocations;
  std::set<uint32_t> code_to_data, data_to_data;
  const auto contains_target = [&](uint32_t value, bool in_data) {
    const uint32_t target_base = in_data ? data_base : base;
    const uint32_t target_size =
        in_data ? data_size + bss_size : application_size;
    return value >= target_base && Within(target_size, value - target_base, 1);
  };
  if (relocation_offset != 0) {
    const size_t end =
        data_relocation_offset ? data_relocation_offset : bytes.size();
    if (relocation_offset != consumed || end <= consumed ||
        end > bytes.size()) {
      return absl::DataLossError("Missing/misplaced E32 code relocations");
    }
    auto decoded = internal::DecodeCodeRelocations(
        bytes.substr(consumed, end - consumed), size, &code_to_data);
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
      const bool table = std::binary_search(expected_relocations.begin(),
                                            expected_relocations.end(), offset);
      const uint32_t value = Read32(bytes, header_size + offset);
      const bool descriptor_limit =
          descriptor != 0 && offset == (descriptor & ~uint32_t{1}) + 12 &&
          value == base + size;
      if (import_slots.contains(offset) ||
          (!Within(application_size, offset, 4) && !table) ||
          (table && code_to_data.contains(offset)) ||
          (!descriptor_limit &&
           !contains_target(value, code_to_data.contains(offset)))) {
        return absl::DataLossError(
            "E32 pointer relocation outside its target mapping");
      }
    }
    consumed = end;
  } else if (dll) {
    return absl::DataLossError("DLL lacks export relocations");
  }
  if (data_relocation_offset != 0) {
    if (data_relocation_offset != consumed || !data_size ||
        consumed >= bytes.size()) {
      return absl::DataLossError("Missing/misplaced E32 data relocations");
    }
    auto decoded = internal::DecodeCodeRelocations(bytes.substr(consumed),
                                                   data_size, &data_to_data);
    if (!decoded.ok()) {
      return decoded.status();
    }
    data_relocations = std::move(*decoded);
    for (const auto offset : data_relocations) {
      if (!contains_target(Read32(bytes, data_offset + offset),
                           data_to_data.contains(offset))) {
        return absl::DataLossError(
            "E32 data pointer relocation outside its target mapping");
      }
    }
    consumed = bytes.size();
  }
  if (consumed != bytes.size()) {
    return absl::DataLossError("Unexpected E32 trailing data");
  }
  return ImageInfo{
      .uid3 = uid3,
      .header_crc = Read32(bytes, 20),
      .flags = flags,
      .architecture = Read16(bytes, 122) == 0x2002 ? "armv6" : "armv5t",
      .code_size = size,
      .code_base = base,
      .data_size = data_size,
      .bss_size = bss_size,
      .data_base = data_base,
      .entry_offset = Read32(bytes, 72),
      .secure_id = Read32(bytes, 128),
      .capabilities = Read32(bytes, 136),
      .dll = dll,
      .header_size = header_size,
      .exception_descriptor_offset = descriptor & ~uint32_t{1},
      .imports = std::move(imports),
      .exports = std::move(exports),
      .code_relocations = std::move(relocations),
      .code_data_relocations = {code_to_data.begin(), code_to_data.end()},
      .data_relocations = std::move(data_relocations),
      .data_data_relocations = {data_to_data.begin(), data_to_data.end()}};
}

}  // namespace symbian::e32
