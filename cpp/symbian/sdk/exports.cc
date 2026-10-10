#include "symbian/sdk/exports.h"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <absl/base/nullability.h>
#include <absl/status/status.h>
#include <absl/strings/str_cat.h>
#include <absl/strings/str_split.h>
#include <absl/strings/strip.h>

#include "symbian/analysis/bytes.h"
#include "symbian/analysis/elf.h"

namespace symbian::sdk {
namespace {

using analysis::internal::Read16;
using analysis::internal::Read32;
using analysis::internal::Within;

bool SymbolName(std::string_view value) {
  if (value.empty() || value.size() > 512) {
    return false;
  }
  for (char c : value) {
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' ||
          (c >= '0' && c <= '9') || c == '.')) {
      return false;
    }
  }
  return !((value[0] >= '0' && value[0] <= '9') || value[0] == '.');
}

bool FileName(std::string_view name, std::string_view suffix) {
  if (name.size() <= suffix.size() || name.size() > 128 ||
      !name.ends_with(suffix)) {
    return false;
  }
  for (const char c : name) {
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.')) {
      return false;
    }
  }
  return name[0] != '.';
}

absl::StatusOr<uint32_t> Number(std::string_view text) {
  uint32_t value = 0;
  if (const auto result =
          std::from_chars(text.data(), text.data() + text.size(), value);
      result.ec != std::errc{} || result.ptr != text.data() + text.size() ||
      value == 0 || value > 65535) {
    return absl::InvalidArgumentError(
        "DEF requires positive decimal numbers in 1..65535");
  }
  return value;
}

struct Section {
  uint32_t type;
  uint32_t address;
  uint32_t offset;
  uint32_t size;
  uint32_t link;
  uint32_t entry_size;
  std::string_view bytes;
};

absl::StatusOr<std::string> Text(std::string_view table, uint32_t offset) {
  if (offset >= table.size()) {
    return absl::DataLossError("ELF string offset exceeds table");
  }
  const size_t end = table.find('\0', offset);
  if (end == std::string_view::npos || end - offset > 512) {
    return absl::DataLossError("Unterminated or oversized ELF string");
  }
  return std::string(table.substr(offset, end - offset));
}

}  // namespace

absl::StatusOr<std::vector<Export>> ParseExports(std::string_view text) {
  if (text.size() > 8 * 1024 * 1024) {
    return absl::ResourceExhaustedError("DEF input exceeds 8 MiB");
  }
  for (char c : text) {
    if (const auto byte = static_cast<uint8_t>(c);
        (byte < 32 && c != '\r' && c != '\n' && c != '\t') || byte > 126) {
      return absl::InvalidArgumentError("DEF requires ASCII text");
    }
  }
  bool header = false;
  std::set<std::string> names;
  std::set<uint32_t> ordinals;
  std::vector<Export> exports;
  for (std::string_view line : absl::StrSplit(text, '\n')) {
    line = absl::StripAsciiWhitespace(line.substr(0, line.find(';')));
    if (line.empty()) {
      continue;
    }
    if (!header) {
      if (line != "EXPORTS") {
        return absl::InvalidArgumentError("DEF requires EXPORTS header");
      }
      header = true;
      continue;
    }
    std::vector<std::string_view> words =
        absl::StrSplit(line, absl::ByAnyChar(" \t\r"), absl::SkipEmpty());
    if (words.size() >= 2 && words[1].size() > 1 && words[1][0] == '@') {
      const std::string_view ordinal = words[1].substr(1);
      words[1] = "@";
      words.insert(words.begin() + 2, ordinal);
    }
    // Nokia's Avkon frozen table omits NONAME on removed exports. These are
    // ordinal tombstones, never named/linkable exports. Keep live exports
    // strict while retaining the original holes without rewriting the DEF.
    const bool unnamed_tombstone = words.size() == 4 && words[3] == "ABSENT";
    if (words.size() < 4 || words[1] != "@" ||
        (words[3] != "NONAME" && !unnamed_tombstone) || !SymbolName(words[0])) {
      return absl::UnimplementedError("Unsupported DEF export declaration");
    }
    const auto ordinal = Number(words[2]);
    if (!ordinal.ok()) {
      return ordinal.status();
    }
    Export item{.symbol = std::string(words[0]), .ordinal = *ordinal};
    item.absent = unnamed_tombstone;
    for (size_t i = 4; i < words.size(); ++i) {
      if (words[i] == "ABSENT" && !item.absent) {
        item.absent = true;
      } else if (words[i] == "DATA" && !item.data && i + 1 < words.size()) {
        if (const auto size = Number(words[++i]); !size.ok()) {
          return size.status();
        }
        item.data = true;
      } else {
        return absl::UnimplementedError("Unsupported DEF export qualifier");
      }
    }
    // Older frozen DEFs omit DATA on Itanium ABI type-info and vtable
    // objects. Their mangling establishes object identity independently of
    // firmware, and they must never become callable PLT symbols.
    if (item.symbol.starts_with("_ZTV") || item.symbol.starts_with("_ZTI") ||
        item.symbol.starts_with("_ZTS")) {
      item.data = true;
    }
    // Original Khronos DEFs reuse placeholder names for absent ordinal slots.
    // Tombstones have no linkable symbol; only their ordinals must be unique.
    if ((!item.absent && !names.insert(item.symbol).second) ||
        !ordinals.insert(item.ordinal).second) {
      return absl::InvalidArgumentError("Duplicate DEF symbol or ordinal");
    }
    exports.push_back(std::move(item));
  }
  if (!header || exports.empty()) {
    return absl::InvalidArgumentError("DEF contains no exports");
  }
  std::sort(
      exports.begin(), exports.end(),
      [](const Export& a, const Export& b) { return a.ordinal < b.ordinal; });
  return exports;
}

absl::StatusOr<std::string> GenerateExportDefinition(std::string_view elf) {
  if (elf.size() > 64 * 1024 * 1024) {
    return absl::ResourceExhaustedError("DLL ELF exceeds 64 MiB");
  }
  const auto header = analysis::InspectElf32(elf);
  if (!header.ok()) {
    return header.status();
  }
  if (header->machine != 40 || header->type != 2) {
    return absl::InvalidArgumentError("DLL exports require a linked ARM ELF");
  }
  std::set<std::string> names;
  bool found_table = false;
  const uint32_t sections = Read32(elf, 32);
  const uint16_t stride = Read16(elf, 46);
  for (size_t index = 1; index < header->section_count; ++index) {
    const size_t p = sections + index * stride;
    if (Read32(elf, p + 4) != 2) {
      continue;
    }
    if (found_table) {
      return absl::DataLossError("Multiple DLL symbol tables");
    }
    found_table = true;
    const uint32_t offset = Read32(elf, p + 16);
    const uint32_t size = Read32(elf, p + 20);
    const uint32_t strings_index = Read32(elf, p + 24);
    if (Read32(elf, p + 36) != 16 || size % 16 ||
        !Within(elf.size(), offset, size) ||
        strings_index >= header->section_count) {
      return absl::DataLossError("Invalid DLL symbol table");
    }
    const size_t strings_header = sections + strings_index * stride;
    const uint32_t strings_offset = Read32(elf, strings_header + 16);
    const uint32_t strings_size = Read32(elf, strings_header + 20);
    if (Read32(elf, strings_header + 4) != 3 ||
        !Within(elf.size(), strings_offset, strings_size)) {
      return absl::DataLossError("Invalid DLL symbol names");
    }
    const auto strings = elf.substr(strings_offset, strings_size);
    for (size_t i = 1; i < size / 16; ++i) {
      const size_t symbol = offset + i * 16;
      const uint8_t info = static_cast<uint8_t>(elf[symbol + 12]);
      const uint16_t owner = Read16(elf, symbol + 14);
      if ((info >> 4 != 1 && info >> 4 != 2) || elf[symbol + 13] != 0 ||
          owner == 0 || owner >= header->section_count) {
        continue;
      }
      const auto name = Text(strings, Read32(elf, symbol));
      if (!name.ok()) {
        return name.status();
      }
      if (*name == "_E32Startup" || !SymbolName(*name)) {
        continue;
      }
      const uint32_t flags = Read32(elf, sections + owner * stride + 8);
      if ((flags & 2) == 0 || (info & 15) == 0) {
        continue;
      }
      // Inline/template constant objects are compiler-generated weak storage,
      // not standalone DLL function exports. Their local storage stays in the
      // implementation image; exporting application DATA remains unsupported.
      if ((info >> 4) == 2 && (info & 15) == 1) {
        continue;
      }
      if ((info & 15) != 2 || (flags & 6) != 6) {
        return absl::UnimplementedError(
            absl::StrCat("Automatic DLL exports require functions: ", *name));
      }
      names.insert(*name);
    }
  }
  if (names.empty() || names.size() > 65535) {
    return absl::FailedPreconditionError("DLL needs 1..65535 visible exports");
  }
  std::string definition = "EXPORTS\n";
  uint32_t ordinal = 0;
  for (const std::string& name : names) {
    absl::StrAppend(&definition, name, " @ ", ++ordinal, " NONAME\n");
  }
  return definition;
}

absl::StatusOr<ProxySources> GenerateProxy(
    std::string_view definition, const std::vector<std::string>& symbols,
    std::string_view soname, std::string_view target_dll) {
  if (!FileName(soname, ".dso") || !FileName(target_dll, ".dll")) {
    return absl::InvalidArgumentError(
        "Proxy requires plain .dso/.dll basenames");
  }
  if (symbols.size() > 65535) {
    return absl::ResourceExhaustedError(
        "Proxy selection exceeds 65535 exports");
  }
  const auto table = ParseExports(definition);
  if (!table.ok()) {
    return table.status();
  }
  ProxySources result;
  if (symbols.empty()) {
    // A library target exposes its complete frozen ABI, excluding tombstones.
    for (const Export& item : *table) {
      if (!item.absent) {
        result.exports.push_back(item);
      }
    }
  } else {
    std::map<std::string_view, const Export* absl_nonnull> by_name;
    for (const Export& item : *table) {
      if (const auto match = by_name.find(item.symbol);
          match == by_name.end() || !item.absent) {
        by_name[item.symbol] = &item;
      }
    }
    std::set<std::string> selected;
    for (const std::string& symbol : symbols) {
      if (!selected.insert(symbol).second) {
        return absl::InvalidArgumentError("Duplicate proxy selection");
      }
      const auto match = by_name.find(symbol);
      if (match == by_name.end()) {
        return absl::NotFoundError(
            absl::StrCat("DEF export missing: ", symbol));
      }
      if (match->second->absent) {
        return absl::FailedPreconditionError("Selected export is absent");
      }
      result.exports.push_back(*match->second);
    }
  }
  if (result.exports.empty()) {
    return absl::FailedPreconditionError("Library contains no present exports");
  }
  std::sort(
      result.exports.begin(), result.exports.end(),
      [](const Export& a, const Export& b) { return a.ordinal < b.ordinal; });
  result.assembly =
      ".syntax unified\n.arm\n.section ER_RO,\"ax\",%progbits\n.balign 4\n";
  result.version_script = absl::StrCat(target_dll, " { global:\n");
  for (const Export& item : result.exports) {
    absl::StrAppend(&result.assembly, ".global ", item.symbol, "\n.type ",
                    item.symbol, item.data ? ", %object\n" : ", %function\n",
                    item.symbol, ":\n.word ", item.ordinal, "\n.size ",
                    item.symbol, ", 4\n");
    absl::StrAppend(&result.version_script, "  ", item.symbol, ";\n");
  }
  result.version_script += "local: *; };\n";
  // The historical consumer treats dynamic addresses as FILE OFFSETS and
  // requires ordinal symbols in section one. LLD's default script does neither.
  result.linker_script =
      R"(PHDRS { code PT_LOAD FLAGS(5); dynamic PT_DYNAMIC FLAGS(4); }
SECTIONS {
 . = SIZEOF_HEADERS;
 .text : { *(ER_RO) } :code
 .dynsym : { *(.dynsym) } :code
 .gnu.version : { *(.gnu.version) } :code
 .gnu.version_d : { *(.gnu.version_d) } :code
 .hash : { *(.hash) } :code
 .dynstr : { *(.dynstr) } :code
 .dynamic : { *(.dynamic) } :code :dynamic
 /DISCARD/ : { *(.comment) *(.ARM.attributes) }
}
)";
  return result;
}

absl::StatusOr<ProxyInfo> InspectProxy(std::string_view bytes) {
  if (bytes.size() > 32 * 1024 * 1024) {
    return absl::ResourceExhaustedError("Proxy exceeds 32 MiB");
  }
  const auto header = analysis::InspectElf32(bytes);
  if (!header.ok()) {
    return header.status();
  }
  if (header->type != 3 || header->machine != 40 ||
      header->flags != 0x05000200 || header->program_count != 2 ||
      header->section_count < 8) {
    return absl::UnimplementedError(
        "Requires generated ARM soft-float ordinal proxy");
  }
  std::vector<Section> sections;
  size_t symbols_index = 0, versions_index = 0, definitions_index = 0,
         dynamic_index = 0;
  for (size_t i = 0; i < header->section_count; ++i) {
    const size_t p = Read32(bytes, 32) + i * Read16(bytes, 46);
    Section section{.type = Read32(bytes, p + 4),
                    .address = Read32(bytes, p + 12),
                    .offset = Read32(bytes, p + 16),
                    .size = Read32(bytes, p + 20),
                    .link = Read32(bytes, p + 24),
                    .entry_size = Read32(bytes, p + 36),
                    .bytes = {}};
    if (!Within(bytes.size(), section.offset, section.size)) {
      return absl::DataLossError("Proxy section exceeds file");
    }
    section.bytes = bytes.substr(section.offset, section.size);
    sections.push_back(section);
    size_t* absl_nullable index = nullptr;
    if (section.type == 11) {
      index = &symbols_index;
    }
    if (section.type == 0x6fffffff) {
      index = &versions_index;
    }
    if (section.type == 0x6ffffffd) {
      index = &definitions_index;
    }
    if (section.type == 6) {
      index = &dynamic_index;
    }
    if (index != nullptr) {
      if (*index != 0) {
        return absl::DataLossError("Duplicate proxy metadata section");
      }
      *index = i;
    }
  }
  if (!symbols_index || !versions_index || !definitions_index ||
      !dynamic_index) {
    return absl::DataLossError("Missing proxy metadata");
  }
  const Section& code = sections[1];
  const Section& symbols = sections[symbols_index];
  const Section& versions = sections[versions_index];
  const Section& definitions = sections[definitions_index];
  const Section& dynamic = sections[dynamic_index];
  if (code.type != 1 || code.size == 0 || code.size % 4 ||
      code.address != code.offset || symbols.entry_size != 16 ||
      symbols.size % 16 || symbols.size < 32 || symbols.size > 65536 * 16 ||
      symbols.link >= sections.size() || versions.size != symbols.size / 8 ||
      definitions.size != 56 || dynamic.size % 8 ||
      dynamic.link != symbols.link) {
    return absl::DataLossError("Invalid proxy metadata layout");
  }
  const Section& strings = sections[symbols.link];
  if (strings.type != 3) {
    return absl::DataLossError("Proxy symbols require a string table");
  }
  for (size_t i = 0; i < header->program_count; ++i) {
    const size_t p = Read32(bytes, 28) + i * Read16(bytes, 42);
    if (!Within(bytes.size(), Read32(bytes, p + 4), Read32(bytes, p + 16))) {
      return absl::DataLossError("Proxy segment exceeds file");
    }
    if (i == 0 && (Read32(bytes, p) != 1 || Read32(bytes, p + 24) != 5 ||
                   Read32(bytes, p + 4) != Read32(bytes, p + 8) ||
                   !Within(Read32(bytes, p + 16),
                           code.offset - Read32(bytes, p + 4), code.size))) {
      return absl::DataLossError(
          "Proxy ordinal segment violates historical mapping");
    }
    if (i == 1 &&
        (Read32(bytes, p) != 2 || Read32(bytes, p + 4) != dynamic.offset ||
         Read32(bytes, p + 16) != dynamic.size)) {
      return absl::DataLossError("Invalid proxy dynamic segment");
    }
  }
  ProxyInfo info;
  // Exactly the base soname definition and one DLL import version.
  for (size_t i = 0; i < 2; ++i) {
    const size_t p = i * 28;
    if (Read16(definitions.bytes, p) != 1 ||
        Read16(definitions.bytes, p + 2) != (i == 0 ? 1 : 0) ||
        Read16(definitions.bytes, p + 4) != i + 1 ||
        Read16(definitions.bytes, p + 6) != 1 ||
        Read32(definitions.bytes, p + 12) != 20 ||
        Read32(definitions.bytes, p + 16) != (i == 0 ? 28 : 0) ||
        Read32(definitions.bytes, p + 24) != 0) {
      return absl::DataLossError("Invalid proxy version definition");
    }
    const auto name = Text(strings.bytes, Read32(definitions.bytes, p + 20));
    if (!name.ok()) {
      return name.status();
    }
    (i == 0 ? info.soname : info.target_dll) = *name;
  }
  if (!FileName(info.soname, ".dso") || !FileName(info.target_dll, ".dll")) {
    return absl::UnimplementedError("Unsupported proxy DLL name");
  }
  std::set<uint32_t> tags;
  for (size_t i = 0; i < dynamic.size; i += 8) {
    const uint32_t tag = Read32(dynamic.bytes, i);
    const uint32_t value = Read32(dynamic.bytes, i + 4);
    if (!tags.insert(tag).second) {
      return absl::DataLossError("Duplicate proxy dynamic tag");
    }
    uint32_t expected = 0;
    switch (tag) {
      case 0:
        if (i + 8 != dynamic.size) {
          return absl::DataLossError("Early proxy DT_NULL");
        }
        break;
      case 4: {
        auto hash = std::find_if(sections.begin(), sections.end(),
                                 [](const Section& s) { return s.type == 5; });
        if (hash == sections.end()) {
          return absl::DataLossError("Missing proxy ELF hash");
        }
        expected = hash->offset;
        break;
      }
      case 5:
        expected = strings.offset;
        break;
      case 6:
        expected = symbols.offset;
        break;
      case 10:
        expected = strings.size;
        break;
      case 11:
        expected = 16;
        break;
      case 14: {
        const auto name = Text(strings.bytes, value);
        if (!name.ok()) {
          return name.status();
        }
        if (*name != info.soname) {
          return absl::DataLossError("Proxy soname mismatch");
        }
        expected = value;
        break;
      }
      case 0x6ffffff0:
        expected = versions.offset;
        break;
      case 0x6ffffffc:
        expected = definitions.offset;
        break;
      case 0x6ffffffd:
        expected = 2;
        break;
      default:
        return absl::UnimplementedError("Unsupported proxy dynamic tag");
    }
    if (value != expected) {
      return absl::DataLossError(
          "Proxy dynamic pointer is not the historical file offset");
    }
  }
  if (tags.size() != 10) {
    return absl::DataLossError("Missing proxy dynamic tags");
  }
  std::set<std::string> names;
  std::set<uint32_t> ordinals;
  if (symbols.bytes.substr(0, 16) != std::string(16, '\0') ||
      Read16(versions.bytes, 0) != 0 ||
      code.size != (symbols.size / 16 - 1) * 4) {
    return absl::DataLossError(
        "Invalid proxy null symbol or ordinal table size");
  }
  std::set<uint32_t> locations;
  for (size_t i = 1; i < symbols.size / 16; ++i) {
    const size_t p = i * 16;
    const uint32_t address = Read32(symbols.bytes, p + 4);
    if (Read16(symbols.bytes, p + 14) != 1 ||
        (static_cast<uint8_t>(symbols.bytes[p + 12]) != 0x12 &&
         static_cast<uint8_t>(symbols.bytes[p + 12]) != 0x11) ||
        symbols.bytes[p + 13] != 0 || Read32(symbols.bytes, p + 8) != 4 ||
        Read16(versions.bytes, i * 2) != 2 || address % 4 ||
        address < code.address ||
        !Within(code.size, address - code.address, 4) ||
        !locations.insert(address).second) {
      return absl::DataLossError("Invalid proxy ordinal symbol");
    }
    const auto name = Text(strings.bytes, Read32(symbols.bytes, p));
    if (!name.ok()) {
      return name.status();
    }
    const uint32_t ordinal = Read32(code.bytes, address - code.address);
    if (!SymbolName(*name) || ordinal == 0 || ordinal > 65535 ||
        !names.insert(*name).second || !ordinals.insert(ordinal).second) {
      return absl::DataLossError("Invalid proxy name or ordinal");
    }
    info.exports.push_back(Export{.symbol = *name,
                                  .ordinal = ordinal,
                                  .data = symbols.bytes[p + 12] == 0x11});
  }
  std::sort(
      info.exports.begin(), info.exports.end(),
      [](const Export& a, const Export& b) { return a.ordinal < b.ordinal; });
  return info;
}

}  // namespace symbian::sdk
