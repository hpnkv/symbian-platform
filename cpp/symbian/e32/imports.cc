#include "symbian/e32/imports.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <absl/status/status.h>
#include <absl/strings/ascii.h>

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
  if (end == std::string_view::npos || end - offset > 512) {
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
  if (proxies.empty() || proxies.size() > 16) {
    return absl::InvalidArgumentError("Supply 1..16 ordinal proxies");
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
  if (symbols.size < 32 || symbols.size > 1025 * 16 ||
      symbols.entry_size != 16 || symbols.size % 16 ||
      symbols.link >= sections.size() || dynamic.link != symbols.link ||
      versions.link != indices.at(11) || needs.link != symbols.link ||
      versions.size != symbols.size / 8 || dynamic.size % 8 ||
      needs.size != libraries.size() * 32 || needs.info != libraries.size()) {
    return absl::DataLossError("Invalid ELF import metadata layout");
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
  for (size_t i = 0; i < libraries.size(); ++i) {
    const size_t p = needs.offset + i * 16;
    const size_t aux = needs.offset + (libraries.size() + i) * 16;
    if (Read16(elf, p) != 1 || Read16(elf, p + 2) != 1 ||
        Read32(elf, p + 8) != libraries.size() * 16 ||
        Read32(elf, p + 12) != (i + 1 == libraries.size() ? 0 : 16) ||
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
      if (result.relocation_index != 0) {
        return absl::UnimplementedError("Multiple dynamic relocation tables");
      }
      result.relocation_index = i;
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
  const std::map<uint32_t, uint32_t> expected{
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
      {0x6fffffff, static_cast<uint32_t>(libraries.size())}};
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
    if (Read32(elf, sym + 4) != 0 || Read32(elf, sym + 8) != 0 ||
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
    blocks[library.target_dll].push_back(
        {location - code.address, function->ordinal});
  }
  if (symbol_indices.size() + 1 != symbols.size / 16 ||
      blocks.size() != libraries.size()) {
    return absl::UnimplementedError("Unreferenced dynamic symbols/proxies");
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
  if (type == 28) {
    if (location % 4) {
      return absl::DataLossError("ARM import instruction is unaligned");
    }
    const uint32_t word = Read32(elf, p);
    if ((word & 0xff000000) != 0xeb000000 &&
        (word & 0xfe000000) != 0xfa000000) {
      return absl::UnimplementedError("Requires linked ARM BL/BLX import call");
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
      const uint32_t ordinal = Read32(code, offset);
      if (ordinal == 0 || ordinal > 65535) {
        return absl::UnimplementedError(
            "Requires function ordinals/zero addends");
      }
      block.slots.push_back({offset, ordinal});
    }
    blocks.push_back(std::move(block));
  }
  if (EncodeImports(blocks) != section) {
    return absl::DataLossError("Noncanonical E32 import section");
  }
  return blocks;
}

}  // namespace symbian::e32::internal
