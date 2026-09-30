#include "symbian/sdk/exports.h"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <vector>

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
  for (char c : name) {
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.')) {
      return false;
    }
  }
  return name[0] != '.';
}

absl::StatusOr<uint32_t> Number(std::string_view text) {
  uint32_t value = 0;
  const auto result =
      std::from_chars(text.data(), text.data() + text.size(), value);
  if (result.ec != std::errc{} || result.ptr != text.data() + text.size() ||
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
    const auto byte = static_cast<uint8_t>(c);
    if ((byte < 32 && c != '\r' && c != '\n' && c != '\t') || byte > 126) {
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
    if (words.size() < 4 || words[1] != "@" || words[3] != "NONAME" ||
        !SymbolName(words[0])) {
      return absl::UnimplementedError("Unsupported DEF export declaration");
    }
    const auto ordinal = Number(words[2]);
    if (!ordinal.ok()) {
      return ordinal.status();
    }
    Export item{std::string(words[0]), *ordinal};
    for (size_t i = 4; i < words.size(); ++i) {
      if (words[i] == "ABSENT" && !item.absent) {
        item.absent = true;
      } else if (words[i] == "DATA" && !item.data && i + 1 < words.size()) {
        const auto size = Number(words[++i]);
        if (!size.ok()) {
          return size.status();
        }
        item.data = true;
      } else {
        return absl::UnimplementedError("Unsupported DEF export qualifier");
      }
    }
    if (!names.insert(item.symbol).second ||
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

absl::StatusOr<ProxySources> GenerateProxy(
    std::string_view definition, const std::vector<std::string>& symbols,
    std::string_view soname, std::string_view target_dll) {
  if (!FileName(soname, ".dso") || !FileName(target_dll, ".dll")) {
    return absl::InvalidArgumentError(
        "Proxy requires plain .dso/.dll basenames");
  }
  if (symbols.empty() || symbols.size() > 256) {
    return absl::InvalidArgumentError("Select 1..256 proxy functions");
  }
  const auto table = ParseExports(definition);
  if (!table.ok()) {
    return table.status();
  }
  ProxySources result;
  std::set<std::string> selected;
  for (const std::string& symbol : symbols) {
    if (!selected.insert(symbol).second) {
      return absl::InvalidArgumentError("Duplicate proxy selection");
    }
    const auto match =
        std::find_if(table->begin(), table->end(),
                     [&](const Export& e) { return e.symbol == symbol; });
    if (match == table->end()) {
      return absl::NotFoundError(absl::StrCat("DEF export missing: ", symbol));
    }
    if (match->absent || match->data) {
      return absl::UnimplementedError(
          "Absent/data exports cannot become function proxies");
    }
    result.exports.push_back(*match);
  }
  std::sort(
      result.exports.begin(), result.exports.end(),
      [](const Export& a, const Export& b) { return a.ordinal < b.ordinal; });
  result.assembly =
      ".syntax unified\n.arm\n.section ER_RO,\"ax\",%progbits\n.balign 4\n";
  result.version_script = absl::StrCat(target_dll, " { global:\n");
  for (const Export& item : result.exports) {
    absl::StrAppend(&result.assembly, ".global ", item.symbol, "\n.type ",
                    item.symbol, ", %function\n", item.symbol, ":\n.word ",
                    item.ordinal, "\n.size ", item.symbol, ", 4\n");
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
  if (bytes.size() > 2 * 1024 * 1024) {
    return absl::ResourceExhaustedError("Proxy exceeds 2 MiB");
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
    Section section{Read32(bytes, p + 4),
                    Read32(bytes, p + 12),
                    Read32(bytes, p + 16),
                    Read32(bytes, p + 20),
                    Read32(bytes, p + 24),
                    Read32(bytes, p + 36),
                    {}};
    if (!Within(bytes.size(), section.offset, section.size)) {
      return absl::DataLossError("Proxy section exceeds file");
    }
    section.bytes = bytes.substr(section.offset, section.size);
    sections.push_back(section);
    size_t* index = nullptr;
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
      symbols.size % 16 || symbols.size < 32 || symbols.size > 257 * 16 ||
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
        static_cast<uint8_t>(symbols.bytes[p + 12]) != 0x12 ||
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
    info.exports.push_back(Export{*name, ordinal});
  }
  std::sort(
      info.exports.begin(), info.exports.end(),
      [](const Export& a, const Export& b) { return a.ordinal < b.ordinal; });
  return info;
}

}  // namespace symbian::sdk
