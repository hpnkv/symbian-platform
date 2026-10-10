#include "symbian/e32/imports.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <absl/base/nullability.h>
#include <absl/status/status.h>
#include <absl/strings/ascii.h>
#include <absl/strings/str_cat.h>

#include "symbian/analysis/bytes.h"
#include "symbian/sdk/exports.h"

namespace symbian::e32::internal {
namespace {

using analysis::internal::Put32;
using analysis::internal::Read16;
using analysis::internal::Read32;
using analysis::internal::Within;

absl::StatusOr<std::string> StringAt(std::string_view table, size_t offset) {
  if (offset >= table.size()) {
    return absl::DataLossError("Import string outside table");
  }
  const size_t end = table.find('\0', offset);
  // Clang can emit long but valid C++ template names in .symtab. Keep the
  // parser bounded by both the section and a generous single-name limit.
  if (end == std::string_view::npos || end - offset > 16 * 1024) {
    return absl::DataLossError("Unterminated/oversized import string");
  }
  return std::string(table.substr(offset, end - offset));
}

bool DllName(std::string_view name) {
  if (name.size() <= 4 || name.size() > 128 || !name.ends_with(".dll") ||
      name.front() == '.') {
    return false;
  }
  for (char c : name) {
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.')) {
      return false;
    }
  }
  return true;
}

absl::StatusOr<size_t> UniqueSection(const std::vector<Section>& sections,
                                     uint32_t type) {
  size_t index = 0;
  for (size_t i = 1; i < sections.size(); ++i) {
    if (sections[i].type == type) {
      if (index != 0) {
        return absl::DataLossError("Duplicate import metadata section");
      }
      index = i;
    }
  }
  if (index == 0) {
    return absl::DataLossError("Missing import metadata section");
  }
  return index;
}

}  // namespace

absl::StatusOr<std::string> SymbolName(std::string_view elf,
                                       const std::vector<Section>& sections,
                                       const Section& symbols, size_t symbol) {
  if (symbols.link >= sections.size() || symbols.entry_size != 16 ||
      symbols.size % 16 || symbol >= symbols.size / 16) {
    return absl::DataLossError("Invalid import symbol table");
  }
  const Section& strings = sections[symbols.link];
  if (strings.type != 3) {
    return absl::DataLossError("Import symbols need a string table");
  }
  return StringAt(elf.substr(strings.offset, strings.size),
                  Read32(elf, symbols.offset + symbol * 16));
}

absl::StatusOr<ResolvedImports> ResolveImports(
    std::string_view elf, const std::vector<Section>& sections,
    const Segment& code, const std::vector<std::string>& proxies) {
  if (proxies.empty() || proxies.size() > 256) {
    return absl::InvalidArgumentError("Supply 1..256 ordinal proxies");
  }
  std::map<std::string, sdk::ProxyInfo> libraries;
  std::set<std::string> dlls;
  for (const std::string& bytes : proxies) {
    auto proxy = sdk::InspectProxy(bytes);
    if (!proxy.ok()) {
      return proxy.status();
    }
    if (!dlls.insert(absl::AsciiStrToLower(proxy->target_dll)).second ||
        !libraries.emplace(proxy->soname, *proxy).second) {
      return absl::InvalidArgumentError("Duplicate proxy identity");
    }
  }
  std::map<uint32_t, size_t> indices;
  for (uint32_t type : {6U, 11U, 5U, 0x6fffffffU, 0x6ffffffeU}) {
    auto index = UniqueSection(sections, type);
    if (!index.ok()) {
      return index.status();
    }
    indices.emplace(type, *index);
  }
  const Section& dynamic = sections[indices.at(6)];
  const Section& symbols = sections[indices.at(11)];
  const Section& versions = sections[indices.at(0x6fffffff)];
  const Section& needs = sections[indices.at(0x6ffffffe)];
  // The project may declare more compatible proxies than this image uses.
  // Only the ELF's needed/version records become E32 import blocks.
  const size_t needed_count = needs.size / 32;
  if (symbols.size < 32 || symbols.size > 1025 * 16 ||
      symbols.entry_size != 16 || symbols.size % 16 ||
      symbols.link >= sections.size() || dynamic.link != symbols.link ||
      versions.link != indices.at(11) || needs.link != symbols.link ||
      versions.size != symbols.size / 8 || dynamic.size % 8 ||
      needs.size % 32 || needed_count == 0 || needed_count > libraries.size() ||
      needs.info != needed_count) {
    return absl::DataLossError(absl::StrCat(
        "Invalid ELF import metadata layout: needed=", needed_count,
        " proxies=", libraries.size(), " dynsym=", symbols.size,
        " versions=", versions.size, " needed-info=", needs.info));
  }
  const Section& strings = sections[symbols.link];
  if (strings.type != 3 || strings.flags != 2 || symbols.flags != 2 ||
      dynamic.flags != 3 || dynamic.entry_size != 8 || versions.flags != 2 ||
      versions.entry_size != 2 || needs.flags != 2 ||
      sections[indices.at(5)].flags != 2 ||
      elf.substr(symbols.offset, 16) != std::string(16, '\0') ||
      Read16(elf, versions.offset) != 0) {
    return absl::DataLossError("Invalid import strings/null symbol");
  }
  const auto table = elf.substr(strings.offset, strings.size);
  std::map<uint32_t, std::string> version_libraries;
  std::set<std::string> used;
  for (size_t i = 0; i < needed_count; ++i) {
    const size_t p = needs.offset + i * 16;
    const size_t aux = needs.offset + (needed_count + i) * 16;
    if (Read16(elf, p) != 1 || Read16(elf, p + 2) != 1 ||
        Read32(elf, p + 8) != needed_count * 16 ||
        Read32(elf, p + 12) != (i + 1 == needed_count ? 0 : 16) ||
        Read16(elf, aux + 4) != 0 || Read32(elf, aux + 12) != 0) {
      return absl::UnimplementedError("Requires one version per import DLL");
    }
    const auto soname = StringAt(table, Read32(elf, p + 4));
    const auto dll = StringAt(table, Read32(elf, aux + 8));
    if (!soname.ok()) {
      return soname.status();
    }
    if (!dll.ok()) {
      return dll.status();
    }
    const auto library = libraries.find(*soname);
    const uint16_t version = Read16(elf, aux + 6);
    if (library == libraries.end() || library->second.target_dll != *dll ||
        !used.insert(*soname).second || version < 2 || version > 0x7fff ||
        !version_libraries.emplace(version, *soname).second) {
      return absl::FailedPreconditionError("Proxy/version identity mismatch");
    }
  }
  ResolvedImports result;
  result.dynamic_index = indices.at(6);
  for (size_t i = 1; i < sections.size(); ++i) {
    if (sections[i].type == 9 && sections[i].link == indices.at(11)) {
      if (sections[i].name == ".rel.plt" && result.relocation_index == 0) {
        result.relocation_index = i;
      } else if (sections[i].name == ".rel.dyn" &&
                 result.data_relocation_index == 0) {
        result.data_relocation_index = i;
      } else {
        return absl::UnimplementedError("Multiple dynamic relocation tables");
      }
    }
  }
  if (result.relocation_index == 0) {
    return absl::DataLossError("Missing import relocations");
  }
  const Section& relocs = sections[result.relocation_index];
  if (relocs.info >= sections.size() || relocs.info == 0 ||
      relocs.flags != 0x42 || relocs.entry_size != 8 || relocs.size % 8 ||
      relocs.size == 0 || relocs.size > 1024 * 8) {
    return absl::DataLossError("Invalid import relocation table");
  }
  result.got_index = relocs.info;
  const Section& got = sections[result.got_index];
  if (got.type != 1 || got.flags != 3 || got.address % 4 ||
      got.size != 12 + relocs.size / 2 || Read32(elf, got.offset + 4) != 0 ||
      Read32(elf, got.offset + 8) != 0 ||
      (Read32(elf, got.offset) != 0 &&
       Read32(elf, got.offset) != dynamic.address)) {
    return absl::UnimplementedError("Requires generated function GOT/PLT");
  }
  std::set<uint32_t> tags;
  std::set<std::string> needed;
  std::map<uint32_t, uint32_t> expected{
      {0, 0},
      {21, 0},
      {23, relocs.address},
      {2, relocs.size},
      {3, got.address},
      {20, 17},
      {6, symbols.address},
      {11, 16},
      {5, strings.address},
      {10, strings.size},
      {4, sections[indices.at(5)].address},
      {0x6ffffff0, versions.address},
      {0x6ffffffe, needs.address},
      {0x6fffffff, static_cast<uint32_t>(needed_count)}};
  if (result.data_relocation_index != 0) {
    const Section& data_relocs = sections[result.data_relocation_index];
    if (data_relocs.flags != 2 || data_relocs.info != 0 ||
        data_relocs.entry_size != 8 || data_relocs.size == 0 ||
        data_relocs.size > 256 * 8 || data_relocs.size % 8) {
      return absl::DataLossError("Invalid imported data relocation table");
    }
    expected.emplace(17, data_relocs.address);  // DT_REL
    expected.emplace(18, data_relocs.size);     // DT_RELSZ
    expected.emplace(19, 8);                    // DT_RELENT
    bool imported_text_relocation = false;
    for (size_t i = 0; i < data_relocs.size; i += 8) {
      const uint32_t info = Read32(elf, data_relocs.offset + i + 4);
      const uint32_t symbol = info >> 8;
      const uint32_t location = Read32(elf, data_relocs.offset + i);
      if ((info & 255) == 2 && symbol > 0 && symbol < symbols.size / 16 &&
          std::any_of(
              sections.begin(), sections.end(),
              [&](const Section& section) {
                return (section.name == ".rodata" ||
                        section.name == ".text") &&
                       section.type == 1 && (section.flags & 2) != 0 &&
                       location >= section.address &&
                       Within(section.size, location - section.address, 4);
              })) {
        imported_text_relocation = true;
      }
    }
    if (imported_text_relocation) {
      // E32 resolves validated words through ordinal imports. Never copy
      // a proxy object's ordinal bytes or run an ELF dynamic linker.
      expected.emplace(30, 4);  // DT_FLAGS = DF_TEXTREL
      expected.emplace(22, 0);  // DT_TEXTREL
    }
  }
  for (const Section& array : sections) {
    uint32_t address_tag = 0;
    uint32_t size_tag = 0;
    if (array.name == ".init_array") {
      address_tag = 25;
      size_tag = 27;
    } else if (array.name == ".fini_array") {
      address_tag = 26;
      size_tag = 28;
    } else {
      continue;
    }
    if ((array.size == 0 ? array.flags != 2 && array.flags != 3
                         : array.flags != 3) ||
        (array.size != 0 && array.address % 4) || array.size % 4 ||
        array.size > 1024 || array.address < code.address ||
        !Within(code.size, array.address - code.address, array.size) ||
        (array.size == 0 && array.type != 1 &&
         array.type != address_tag - 11) ||
        (array.size != 0 && array.type != address_tag - 11)) {
      return absl::DataLossError(
          absl::StrCat("Invalid ELF lifecycle dynamic metadata: ", array.name,
                       " type=", array.type, " flags=", array.flags,
                       " address=", array.address, " size=", array.size));
    }
    if (!expected.emplace(address_tag, array.address).second ||
        !expected.emplace(size_tag, array.size).second) {
      return absl::DataLossError("Duplicate ELF lifecycle dynamic metadata");
    }
  }
  for (size_t p = dynamic.offset; p < dynamic.offset + dynamic.size; p += 8) {
    const uint32_t tag = Read32(elf, p), value = Read32(elf, p + 4);
    if (tag == 1) {
      auto name = StringAt(table, value);
      if (!name.ok()) {
        return name.status();
      }
      if (!needed.insert(*name).second ||
          libraries.find(*name) == libraries.end()) {
        return absl::FailedPreconditionError("Unknown/duplicate needed proxy");
      }
      continue;
    }
    const auto match = expected.find(tag);
    if (match == expected.end()) {
      return absl::UnimplementedError("Unsupported ELF dynamic contract");
    }
    if (!tags.insert(tag).second || value != match->second ||
        (tag == 0 && p + 8 != dynamic.offset + dynamic.size)) {
      return absl::DataLossError("Invalid ELF dynamic metadata");
    }
  }
  if (tags.size() != expected.size() || needed != used) {
    return absl::DataLossError("Missing ELF dynamic import contract");
  }
  std::map<std::string, std::vector<ImportSlot>> blocks;
  std::set<uint32_t> locations, symbol_indices;
  const Section* absl_nullable plt = nullptr;
  for (const Section& section : sections) {
    if (section.name == ".plt") {
      if (plt != nullptr || section.type != 1 || section.flags != 6 ||
          section.size != 32 + relocs.size * 2 ||
          section.address < code.address ||
          !Within(code.size, section.address - code.address, section.size)) {
        return absl::UnimplementedError("Requires generated ARM function PLT");
      }
      plt = &section;
    }
  }
  for (size_t i = 0; i < relocs.size; i += 8) {
    const uint32_t location = Read32(elf, relocs.offset + i);
    const uint32_t info = Read32(elf, relocs.offset + i + 4);
    const uint32_t symbol = info >> 8;
    if ((info & 0xff) != 22 || symbol == 0 || symbol >= symbols.size / 16 ||
        location < got.address + 12 ||
        !Within(got.size, location - got.address, 4) || location % 4 ||
        !locations.insert(location).second ||
        !symbol_indices.insert(symbol).second) {
      return absl::UnimplementedError(
          "Requires unique R_ARM_JUMP_SLOT imports");
    }
    const size_t sym = symbols.offset + symbol * 16;
    const uint32_t symbol_value = Read32(elf, sym + 4);
    const uint32_t plt_index = static_cast<uint32_t>(i / 8);
    const uint32_t plt_value =
        plt == nullptr ? 0 : plt->address + 32 + plt_index * 16;
    if ((symbol_value != 0 && (plt == nullptr || symbol_value != plt_value)) ||
        Read32(elf, sym + 8) != 0 ||
        static_cast<uint8_t>(elf[sym + 12]) != 0x12 || elf[sym + 13] != 0 ||
        Read16(elf, sym + 14) != 0) {
      return absl::UnimplementedError("Only undefined global function imports");
    }
    const auto version =
        version_libraries.find(Read16(elf, versions.offset + symbol * 2));
    if (version == version_libraries.end()) {
      return absl::DataLossError("Import symbol has unknown DLL version");
    }
    const auto name = SymbolName(elf, sections, symbols, symbol);
    if (!name.ok()) {
      return name.status();
    }
    const auto& library = libraries.at(version->second);
    const auto function =
        std::find_if(library.exports.begin(), library.exports.end(),
                     [&](const sdk::Export& e) { return e.symbol == *name; });
    if (function == library.exports.end() ||
        !result.functions.emplace(*name, location - code.address).second) {
      return absl::FailedPreconditionError(
          "Missing/ambiguous imported function");
    }
    if (plt != nullptr) {
      result.plt_functions.emplace(*name, plt_value);
    }
    blocks[library.target_dll].push_back(
        {location - code.address, function->ordinal});
  }
  // Imported objects use an eager GOT word, just like a function PLT slot.
  // Never copy proxy ordinal bytes into application data (R_ARM_COPY).
  if (result.data_relocation_index != 0) {
    const Section& data_relocs = sections[result.data_relocation_index];
    const Section* absl_nullable object_got = nullptr;
    for (const Section& section : sections) {
      if (section.name == ".got") {
        object_got = &section;
      }
    }
    for (size_t i = 0; i < data_relocs.size; i += 8) {
      const uint32_t location = Read32(elf, data_relocs.offset + i);
      const uint32_t info = Read32(elf, data_relocs.offset + i + 4);
      const uint32_t type = info & 0xff;
      if (type != 21 && type != 2) {  // GLOB_DAT or a read-only object pointer.
        continue;
      }
      const uint32_t symbol = info >> 8;
      if (symbol == 0 || symbol >= symbols.size / 16) {
        return absl::DataLossError("Invalid imported object symbol");
      }
      const size_t p = symbols.offset + symbol * 16;
      const bool function = static_cast<uint8_t>(elf[p + 12]) == 0x12;
      if (type == 2 && function) {
        const auto owner = std::find_if(
            sections.begin(), sections.end(), [&](const Section& section) {
              return (section.name == ".rodata" ||
                      section.name == ".text") &&
                     section.type == 1 && (section.flags & 2) != 0 &&
                     location >= section.address &&
                     Within(section.size, location - section.address, 4);
            });
        if (owner == sections.end()) {
          continue;  // Writable pointers use their validated PLT below.
        }
        if (Read32(elf, p + 4) != 0 || Read32(elf, p + 8) != 0 ||
            elf[p + 13] != 0 || Read16(elf, p + 14) != 0 || location % 4 ||
            !Within(code.size, location - code.address, 4) ||
            Read32(elf, owner->offset + location - owner->address) != 0 ||
            !locations.insert(location).second) {
          return absl::DataLossError(
              "Invalid read-only imported function pointer");
        }
        const auto version =
            version_libraries.find(Read16(elf, versions.offset + symbol * 2));
        if (version == version_libraries.end()) {
          return absl::DataLossError(
              "Imported function has unknown DLL version");
        }
        const auto name = SymbolName(elf, sections, symbols, symbol);
        if (!name.ok()) {
          return name.status();
        }
        const auto& library = libraries.at(version->second);
        const auto item = std::find_if(
            library.exports.begin(), library.exports.end(),
            [&](const sdk::Export& e) { return e.symbol == *name; });
        if (item == library.exports.end() || item->data) {
          return absl::FailedPreconditionError("Missing imported function");
        }
        result.functions.try_emplace(*name, location - code.address);
        result.code_function_pointers.emplace(location, *name);
        symbol_indices.insert(symbol);
        blocks[library.target_dll].push_back(
            {location - code.address, item->ordinal});
        continue;
      }
      if (Read32(elf, p + 4) != 0 ||
          static_cast<uint8_t>(elf[p + 12]) != 0x11 || elf[p + 13] != 0 ||
          Read16(elf, p + 14) != 0) {
        return absl::UnimplementedError(
            "GOT data import requires an undefined object");
      }
      const auto version =
          version_libraries.find(Read16(elf, versions.offset + symbol * 2));
      if (version == version_libraries.end()) {
        return absl::DataLossError("Imported object has unknown DLL version");
      }
      const auto name = SymbolName(elf, sections, symbols, symbol);
      if (!name.ok()) {
        return name.status();
      }
      const auto& library = libraries.at(version->second);
      const auto item =
          std::find_if(library.exports.begin(), library.exports.end(),
                       [&](const sdk::Export& e) { return e.symbol == *name; });
      if (item == library.exports.end() || !item->data) {
        return absl::FailedPreconditionError(
            "Missing/ambiguous imported object");
      }
      if (location % 4 || !Within(code.size, location - code.address, 4) ||
          !locations.insert(location).second) {
        return absl::DataLossError("Invalid imported object code slot");
      }
      uint32_t addend = 0;
      if (type == 21) {
        if (object_got == nullptr || object_got->type != 1 ||
            object_got->flags != 3 || location < object_got->address ||
            !Within(object_got->size, location - object_got->address, 4) ||
            Read32(elf, object_got->offset + location - object_got->address) !=
                0 ||
            result.objects.contains(*name)) {
          return absl::DataLossError("Invalid imported object GOT slot");
        }
        result.object_slots.insert(location - code.address);
      } else {
        const auto owner = std::find_if(
            sections.begin(), sections.end(), [&](const Section& section) {
              return (section.name == ".rodata" ||
                      section.name == ".text") &&
                     section.type == 1 && (section.flags & 2) != 0 &&
                     location >= section.address &&
                     Within(section.size, location - section.address, 4);
            });
        if (owner == sections.end()) {
          return absl::UnimplementedError(
              "Absolute imported objects require read-only pointer storage");
        }
        addend = Read32(elf, owner->offset + location - owner->address);
        if (addend > 65535) {
          return absl::UnimplementedError(
              "Imported object addend exceeds E32's 16-bit field");
        }
        result.code_object_pointers.emplace(location, addend);
      }
      result.objects.try_emplace(*name, location - code.address);
      symbol_indices.insert(symbol);
      blocks[library.target_dll].push_back(
          {location - code.address, item->ordinal, addend});
    }
  }
  // A complete import library can name a function also provided by a static
  // runtime archive. LLD retains that definition in dynsym; it is executable
  // code, not an unresolved loader import. Validate it rather than rejecting
  // the application's ordinary symbol preemption.
  for (size_t symbol = 1; symbol < symbols.size / 16; ++symbol) {
    if (symbol_indices.contains(static_cast<uint32_t>(symbol))) {
      continue;
    }
    const size_t p = symbols.offset + symbol * 16;
    const uint16_t index = Read16(elf, p + 14);
    const uint32_t size = Read32(elf, p + 8);
    const uint8_t info = static_cast<uint8_t>(elf[p + 12]);
    const bool function = (info == 0x12 || info == 0x22);
    const bool weak_object = info == 0x21;
    const uint32_t address =
        Read32(elf, p + 4) & (function ? ~uint32_t{1} : ~uint32_t{0});
    // LLD may retain an unused proxy symbol in dynsym after section GC.
    // All executable/data relocations above have already been resolved, so
    // an undefined symbol absent from symbol_indices has no loader use.
    if (index == 0 && size == 0) {
      continue;
    }
    if (index == 0 || index >= sections.size() || (!function && !weak_object) ||
        elf[p + 13] != 0 || Read16(elf, versions.offset + symbol * 2) != 1 ||
        size == 0 || (sections[index].flags & 2) == 0 ||
        address < sections[index].address ||
        !Within(sections[index].size, address - sections[index].address,
                size) ||
        (function &&
         ((sections[index].flags & 6) != 6 || address < code.address ||
          !Within(code.size, address - code.address, size)))) {
      const auto name = SymbolName(elf, sections, symbols, symbol);
      if (!name.ok()) {
        return name.status();
      }
      return absl::UnimplementedError(absl::StrCat(
          "Unreferenced dynamic symbol is not a defined local function or weak "
          "object: ",
          *name));
    }
  }
  if (blocks.size() != needed_count) {
    return absl::UnimplementedError("Unreferenced dynamic symbols/proxies");
  }
  if (result.data_relocation_index != 0) {
    const Section& data_relocs = sections[result.data_relocation_index];
    for (size_t i = 0; i < data_relocs.size; i += 8) {
      const uint32_t location = Read32(elf, data_relocs.offset + i);
      const uint32_t info = Read32(elf, data_relocs.offset + i + 4);
      const uint32_t symbol = info >> 8;
      if ((info & 0xff) == 21) {
        continue;  // Validated imported object above.
      }
      if (result.code_object_pointers.contains(location)) {
        continue;  // Validated read-only imported object pointer above.
      }
      if (result.code_function_pointers.contains(location)) {
        continue;  // Validated eager read-only function pointer above.
      }
      if ((info & 0xff) != 2 || symbol == 0 ||
          !symbol_indices.contains(symbol) || location % 4) {
        return absl::UnimplementedError(
            "Only imported-function R_ARM_ABS32 data relocations");
      }
      const auto name = SymbolName(elf, sections, symbols, symbol);
      if (!name.ok()) {
        return name.status();
      }
      const auto plt_function = result.plt_functions.find(*name);
      if (plt_function == result.plt_functions.end() ||
          !result.data_function_pointers.emplace(location, plt_function->second)
               .second) {
        return absl::UnimplementedError(absl::StrCat(
            "Imported data pointer lacks a unique function PLT slot: ",
            *name));
      }
    }
  }
  for (auto& [dll, slots] : blocks) {
    std::sort(slots.begin(), slots.end(), [](const auto& a, const auto& b) {
      return a.code_offset < b.code_offset;
    });
    result.blocks.push_back({dll, std::move(slots)});
  }
  return result;
}

absl::Status CheckImportCall(std::string_view elf, const Segment& code,
                             uint32_t location, uint32_t type,
                             uint32_t slot_offset) {
  const size_t p = code.offset + location - code.address;
  int64_t target = 0;
  if (type == 28 || type == 29) {
    if (location % 4) {
      return absl::DataLossError("ARM import instruction is unaligned");
    }
    const uint32_t word = Read32(elf, p);
    if ((type == 28 && (word & 0xff000000) != 0xeb000000 &&
         (word & 0xfe000000) != 0xfa000000) ||
        (type == 29 && (word & 0xff000000) != 0xea000000)) {
      return absl::UnimplementedError("Requires linked ARM branch to import");
    }
    int64_t displacement = static_cast<int64_t>(word & 0xffffff) * 4;
    if (word & 0x800000) {
      displacement -= int64_t{1} << 26;
    }
    if ((word & 0xfe000000) == 0xfa000000) {
      displacement += (word >> 23) & 2;
    }
    target = location + 8 + displacement;
  } else if (type == 10) {
    if (location % 2) {
      return absl::DataLossError("Thumb import instruction is unaligned");
    }
    const uint16_t first = Read16(elf, p), second = Read16(elf, p + 2);
    if ((first & 0xf800) != 0xf000 || (second & 0xf800) != 0xe800) {
      return absl::UnimplementedError("Requires ARMv5 Thumb BLX import call");
    }
    int64_t displacement = ((first & 0x7ff) << 12) | ((second & 0x7ff) << 1);
    if (first & 0x400) {
      displacement -= int64_t{1} << 23;
    }
    target = ((location + 4) & ~uint32_t{3}) + displacement;
  } else {
    return absl::UnimplementedError("Unsupported external call relocation");
  }
  if (target < code.address || target % 4 ||
      !Within(code.size, static_cast<uint64_t>(target - code.address), 12)) {
    return absl::DataLossError("Import call target outside code");
  }
  const size_t t = code.offset + static_cast<size_t>(target - code.address);
  const uint32_t a = Read32(elf, t), b = Read32(elf, t + 4),
                 c = Read32(elf, t + 8);
  if ((a & 0xfffff000) != 0xe28fc000 || (b & 0xfffff000) != 0xe28cc000 ||
      (c & 0xfffff000) != 0xe5bcf000) {
    return absl::UnimplementedError("Requires PC-relative ARM LLD PLT veneer");
  }
  auto immediate = [](uint32_t instruction) {
    const uint32_t value = instruction & 0xff;
    const uint32_t rotation = ((instruction >> 8) & 15) * 2;
    return rotation == 0 ? value
                         : (value >> rotation) | (value << (32 - rotation));
  };
  const uint64_t slot = static_cast<uint64_t>(target) + 8 +
                        uint64_t{immediate(a)} + immediate(b) + (c & 0xfff);
  if (slot != uint64_t{code.address} + slot_offset) {
    return absl::DataLossError("Import call veneer targets the wrong slot");
  }
  return absl::OkStatus();
}

std::string EncodeImports(const std::vector<ImportBlock>& imports) {
  size_t size = 4;
  for (const auto& block : imports) {
    size += 8 + 4 * block.slots.size();
  }
  std::string bytes(size, '\0');
  size_t p = 4;
  for (const auto& block : imports) {
    Put32(bytes, p, static_cast<uint32_t>(bytes.size()));
    Put32(bytes, p + 4, static_cast<uint32_t>(block.slots.size()));
    p += 8;
    for (const auto& slot : block.slots) {
      Put32(bytes, p, slot.code_offset);
      p += 4;
    }
    bytes.append(block.dll);
    bytes.push_back('\0');
  }
  bytes.append((4 - bytes.size() % 4) % 4, '\0');
  Put32(bytes, 0, static_cast<uint32_t>(bytes.size()));
  return bytes;
}

absl::StatusOr<std::vector<ImportBlock>> DecodeImports(std::string_view section,
                                                       std::string_view code,
                                                       uint32_t count) {
  if (count == 0 || count > 16 || section.size() < 4 || section.size() > 8192 ||
      section.size() % 4 || Read32(section, 0) != section.size()) {
    return absl::DataLossError("Invalid E32 import section size/count");
  }
  std::vector<ImportBlock> blocks;
  std::set<uint32_t> locations;
  size_t p = 4;
  for (size_t i = 0; i < count; ++i) {
    if (!Within(section.size(), p, 8)) {
      return absl::DataLossError("Truncated E32 import block");
    }
    const auto dll = StringAt(section, Read32(section, p));
    if (!dll.ok()) {
      return dll.status();
    }
    const uint32_t slots = Read32(section, p + 4);
    p += 8;
    if (!DllName(*dll) || (!blocks.empty() && *dll <= blocks.back().dll) ||
        slots == 0 || slots > 1024 ||
        !Within(section.size(), p, static_cast<size_t>(slots) * 4)) {
      return absl::DataLossError("Invalid E32 import block name/count");
    }
    ImportBlock block{*dll, {}};
    for (size_t j = 0; j < slots; ++j, p += 4) {
      const uint32_t offset = Read32(section, p);
      if (offset < 16 || offset % 4 || !Within(code.size(), offset, 4) ||
          !locations.insert(offset).second || locations.size() > 1024 ||
          (!block.slots.empty() && offset <= block.slots.back().code_offset)) {
        return absl::DataLossError("Invalid E32 import code offset");
      }
      const uint32_t word = Read32(code, offset);
      const uint32_t ordinal = word & 65535;
      if (ordinal == 0) {
        return absl::UnimplementedError(
            "Requires a nonzero E32 import ordinal");
      }
      block.slots.push_back({offset, ordinal, word >> 16});
    }
    blocks.push_back(std::move(block));
  }
  if (EncodeImports(blocks) != section) {
    return absl::DataLossError("Noncanonical E32 import section");
  }
  return blocks;
}

std::string EncodePeImports(const std::vector<ImportBlock>& imports) {
  auto words = imports;
  for (auto& block : words) {
    for (auto& slot : block.slots) {
      slot.code_offset = slot.ordinal;
    }
  }
  return EncodeImports(words);
}

absl::StatusOr<std::vector<ImportBlock>> DecodePeImports(
    std::string_view section, std::string_view code, uint32_t text_size,
    uint32_t count) {
  if (count == 0 || count > 16 || section.size() < 4 || section.size() > 8192 ||
      section.size() % 4 || code.size() > UINT32_MAX - 4ULL ||
      Read32(section, 0) != section.size() || text_size < 16 || text_size % 4 ||
      !Within(code.size(), text_size, 4) || (code.size() - text_size) % 4) {
    return absl::DataLossError("Invalid PE import section/IAT layout");
  }
  std::vector<ImportBlock> blocks;
  size_t p = 4;
  uint32_t iat = text_size;
  for (uint32_t i = 0; i < count; ++i) {
    if (!Within(section.size(), p, 8)) {
      return absl::DataLossError("Truncated PE import block");
    }
    const auto dll = StringAt(section, Read32(section, p));
    if (!dll.ok()) {
      return dll.status();
    }
    const uint32_t slots = Read32(section, p + 4);
    p += 8;
    if (!DllName(*dll) || (!blocks.empty() && *dll <= blocks.back().dll) ||
        slots == 0 || slots > 1024 ||
        !Within(section.size(), p, uint64_t{slots} * 4) ||
        !Within(code.size(), iat, uint64_t{slots} * 4 + 4)) {
      return absl::DataLossError("Invalid PE DLL/ordinal count");
    }
    ImportBlock block{*dll, {}};
    for (uint32_t j = 0; j < slots; ++j, p += 4, iat += 4) {
      const uint32_t ordinal = Read32(section, p);
      if (ordinal == 0 || ordinal > 65535 || ordinal != Read32(code, iat)) {
        return absl::DataLossError("PE ordinal/IAT mismatch");
      }
      block.slots.push_back({iat, ordinal});
    }
    blocks.push_back(std::move(block));
  }
  if (iat + 4 != code.size() || Read32(code, iat) != 0 ||
      EncodePeImports(blocks) != section) {
    return absl::DataLossError("Noncanonical PE imports/IAT terminator");
  }
  return blocks;
}

}  // namespace symbian::e32::internal
