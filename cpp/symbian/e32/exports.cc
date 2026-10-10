#include "symbian/e32/exports.h"

#include <algorithm>
#include <map>
#include <string>
#include <utility>

#include <absl/base/nullability.h>
#include <absl/status/status.h>
#include <absl/strings/str_cat.h>

#include "symbian/analysis/bytes.h"
#include "symbian/sdk/exports.h"

namespace symbian::e32::internal {
namespace {
using analysis::internal::Put16;
using analysis::internal::Put32;
using analysis::internal::Read16;
using analysis::internal::Read32;
using analysis::internal::Within;
}  // namespace

absl::StatusOr<std::vector<ExportSlot>> ResolveExports(
    std::string_view elf, const std::vector<Section>& sections,
    const Segment& code, uint32_t entry, std::string_view definition) {
  const auto table = sdk::ParseExports(definition);
  if (!table.ok()) {
    return table.status();
  }
  std::vector<ExportSlot> exports(table->back().ordinal);
  const uint32_t absent = code.address + entry;
  for (size_t i = 0; i < exports.size(); ++i) {
    exports[i] = {.ordinal = static_cast<uint32_t>(i + 1),
                  .address = absent,
                  .absent = true};
  }
  std::map<std::string, uint32_t> wanted;
  for (const auto& item : *table) {
    if (item.data) {
      return absl::UnimplementedError("DLL data exports unsupported");
    }
    if (!item.absent) {
      wanted.emplace(item.symbol, item.ordinal);
    }
  }
  const Section* absl_nullable symbols = nullptr;
  for (const auto& section : sections) {
    if (section.type == 2) {
      if (symbols != nullptr) {
        return absl::DataLossError("Multiple ELF static symbol tables");
      }
      symbols = &section;
    }
  }
  if (symbols == nullptr || symbols->entry_size != 16 || symbols->size % 16 ||
      symbols->size > 16 * 1024 * 1024) {
    return absl::DataLossError("Invalid DLL symbol table");
  }
  for (size_t i = 1; i < symbols->size / 16; ++i) {
    const size_t p = symbols->offset + i * 16;
    const uint8_t info = static_cast<uint8_t>(elf[p + 12]);
    if (info >> 4 != 1 && info >> 4 != 2) {
      continue;
    }
    const auto name = SymbolName(elf, sections, *symbols, i);
    if (!name.ok()) {
      return name.status();
    }
    const auto match = wanted.find(*name);
    if (match == wanted.end()) {
      continue;
    }
    const uint16_t index = Read16(elf, p + 14);
    const uint32_t address = Read32(elf, p + 4);
    const uint32_t normalized = address & ~uint32_t{1};
    if (const uint32_t size = Read32(elf, p + 8);
        (info & 15) != 2 || elf[p + 13] != 0 || index == 0 ||
        index >= sections.size() || (sections[index].flags & 6) != 6 ||
        normalized < code.address || normalized < sections[index].address ||
        size == 0 || !Within(code.size, normalized - code.address, size) ||
        !Within(sections[index].size, normalized - sections[index].address,
                size) ||
        normalized % ((address & 1) ? 2 : 4) || address == absent) {
      return absl::UnimplementedError(
          "DLL export must be a defined visible function");
    }
    ExportSlot& slot = exports[match->second - 1];
    if (!slot.absent) {
      return absl::DataLossError("Duplicate DLL export symbol");
    }
    slot.address = address;
    slot.absent = false;
  }
  for (const auto& [name, ordinal] : wanted) {
    if (exports[ordinal - 1].absent) {
      return absl::NotFoundError(
          absl::StrCat("DLL export definition missing: ", name));
    }
  }
  return exports;
}

std::string ExportBitmap(const std::vector<ExportSlot>& exports) {
  std::string bitmap((exports.size() + 7) / 8, static_cast<char>(0xff));
  for (size_t i = 0; i < exports.size(); ++i) {
    if (exports[i].absent) {
      bitmap[i / 8] = static_cast<char>(static_cast<uint8_t>(bitmap[i / 8]) &
                                        ~(1U << (i % 8)));
    }
  }
  return bitmap;
}

absl::StatusOr<std::string> EncodeCodeRelocations(
    const std::vector<uint32_t>& offsets,
    const std::set<uint32_t>& data_targets) {
  if (offsets.empty() || offsets.size() > 131070) {
    return absl::InvalidArgumentError(
        "Code relocations require 1..131070 offsets");
  }
  std::string bytes(8, '\0');
  for (const auto offset : data_targets) {
    if (!std::binary_search(offsets.begin(), offsets.end(), offset)) {
      return absl::InvalidArgumentError(
          "Data relocation target lacks an offset");
    }
  }
  size_t first = 0;
  while (first < offsets.size()) {
    const uint32_t page = offsets[first] & ~uint32_t{0xfff};
    size_t end = first;
    while (end < offsets.size() && (offsets[end] & ~uint32_t{0xfff}) == page) {
      if (offsets[end] % 4 || (end != 0 && offsets[end] <= offsets[end - 1])) {
        return absl::InvalidArgumentError(
            "Code relocation offsets must increase and align");
      }
      ++end;
    }
    const size_t block_size = 8 + ((end - first + 1) / 2) * 4;
    const size_t start = bytes.size();
    bytes.append(block_size, '\0');
    Put32(bytes, start, page);
    Put32(bytes, start + 4, static_cast<uint32_t>(block_size));
    for (size_t i = first; i < end; ++i) {
      Put16(bytes, start + 8 + (i - first) * 2,
            static_cast<uint16_t>(
                (data_targets.contains(offsets[i]) ? 0x2000 : 0x1000) |
                (offsets[i] & 0xfff)));
    }
    first = end;
  }
  // Relocation size excludes the eight-byte header; import size includes its header.
  Put32(bytes, 0, static_cast<uint32_t>(bytes.size() - 8));
  Put32(bytes, 4, static_cast<uint32_t>(offsets.size()));
  return bytes;
}

absl::StatusOr<std::vector<uint32_t>> DecodeCodeRelocations(
    std::string_view bytes, uint32_t code_size,
    std::set<uint32_t>* absl_nullable data_targets) {
  if (bytes.size() < 16 || bytes.size() > 1024 * 1024 ||
      Read32(bytes, 0) != bytes.size() - 8 || bytes.size() % 4 ||
      Read32(bytes, 4) == 0 || Read32(bytes, 4) > 131070) {
    return absl::DataLossError("Invalid E32 code relocation section");
  }
  std::vector<uint32_t> offsets;
  std::set<uint32_t> data;
  size_t p = 8;
  while (p < bytes.size()) {
    if (!Within(bytes.size(), p, 8)) {
      return absl::DataLossError("Truncated E32 relocation block");
    }
    const uint32_t page = Read32(bytes, p), size = Read32(bytes, p + 4);
    if (page % 4096 || size < 8 || size % 4 || !Within(bytes.size(), p, size)) {
      return absl::DataLossError("Invalid E32 relocation page/block size");
    }
    for (size_t i = 8; i < size; i += 2) {
      const uint16_t word = Read16(bytes, p + i);
      if (word == 0) {
        continue;
      }
      const uint64_t offset = uint64_t{page} + (word & 0xfff);
      const auto type = word & 0xf000;
      if ((type != 0x1000 && (type != 0x2000 || data_targets == nullptr)) ||
          offset % 4 || !Within(code_size, offset, 4) ||
          offsets.size() >= 131070) {
        return absl::DataLossError("Invalid E32 text relocation entry");
      }
      offsets.push_back(static_cast<uint32_t>(offset));
      if (type == 0x2000) {
        data.insert(static_cast<uint32_t>(offset));
      }
    }
    p += size;
  }
  if (offsets.size() != Read32(bytes, 4)) {
    return absl::DataLossError("E32 relocation count mismatch");
  }
  if (const auto encoded = EncodeCodeRelocations(offsets, data);
      !encoded.ok() || *encoded != bytes) {
    return absl::DataLossError("Noncanonical E32 code relocations");
  }
  if (data_targets != nullptr) {
    *data_targets = std::move(data);
  }
  return offsets;
}

}  // namespace symbian::e32::internal
