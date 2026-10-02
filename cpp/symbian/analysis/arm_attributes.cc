#include "symbian/analysis/arm_attributes.h"

#include <cstddef>
#include <cstdint>
#include <set>
#include <string_view>

#include <absl/status/status.h>

#include "symbian/analysis/bytes.h"

namespace symbian::analysis {
namespace {
using internal::Read32;

absl::StatusOr<uint32_t> Uleb(std::string_view bytes, size_t* position) {
  uint32_t value = 0;
  for (unsigned shift = 0; shift < 35; shift += 7) {
    if (*position == bytes.size()) {
      return absl::DataLossError("Truncated ARM attribute ULEB");
    }
    const auto byte = static_cast<uint8_t>(bytes[(*position)++]);
    if (shift == 28 && (byte & 0xf0)) {
      return absl::DataLossError("Overflowing ARM attribute ULEB");
    }
    value |= uint32_t{byte & 0x7fU} << shift;
    if (!(byte & 0x80)) {
      return value;
    }
  }
  return absl::DataLossError("Overflowing ARM attribute ULEB");
}

absl::StatusOr<std::string_view> String(std::string_view bytes,
                                        size_t* position) {
  const auto end = bytes.find('\0', *position);
  if (end == std::string_view::npos) {
    return absl::DataLossError("Unterminated ARM attribute string");
  }
  const auto value = bytes.substr(*position, end - *position);
  *position = end + 1;
  return value;
}
}  // namespace

absl::StatusOr<ArmAttributes> InspectArmAttributes(std::string_view bytes) {
  // ARM's addenda32 attributes format: version, length-prefixed vendor blocks,
  // length-prefixed file/section/symbol subsections and typed attribute values.
  if (bytes.size() > 65536) {
    return absl::ResourceExhaustedError("ARM attributes exceed 64 KiB");
  }
  if (bytes.empty() || bytes[0] != 'A') {
    return absl::DataLossError("Invalid ARM attribute version");
  }
  ArmAttributes result;
  std::set<uint32_t> seen;
  size_t position = 1;
  while (position < bytes.size()) {
    if (bytes.size() - position < 4) {
      return absl::DataLossError("Truncated ARM vendor length");
    }
    const uint32_t length = Read32(bytes, position);
    if (length < 5 || length > bytes.size() - position) {
      return absl::DataLossError("Invalid ARM vendor bounds");
    }
    const auto vendor = bytes.substr(position, length);
    position += length;
    size_t sub = 4;
    auto name = String(vendor, &sub);
    if (!name.ok()) {
      return name.status();
    }
    if (*name != "aeabi") {
      continue;
    }
    while (sub < vendor.size()) {
      const size_t start = sub;
      const auto tag = Uleb(vendor, &sub);
      if (!tag.ok()) {
        return tag.status();
      }
      if (vendor.size() - sub < 4) {
        return absl::DataLossError("Truncated ARM subsection length");
      }
      const uint32_t count = Read32(vendor, sub);
      sub += 4;
      if (count < sub - start || count > vendor.size() - start) {
        return absl::DataLossError("Invalid ARM subsection bounds");
      }
      const auto attributes = vendor.substr(0, start + count);
      if (*tag != 1) {
        sub = start + count;
        continue;
      }
      while (sub < attributes.size()) {
        const auto attribute = Uleb(attributes, &sub);
        if (!attribute.ok()) {
          return attribute.status();
        }
        if (*attribute == 0) {
          return absl::DataLossError("Invalid ARM attribute tag zero");
        }
        if (*attribute == 65) {
          return absl::UnimplementedError(
              "Nested ARM compatibility attributes unsupported");
        }
        if (*attribute == 4 || *attribute == 5 || *attribute == 67 ||
            (*attribute > 32 && (*attribute & 1))) {
          const auto text = String(attributes, &sub);
          if (!text.ok()) {
            return text.status();
          }
          continue;
        }
        if (*attribute == 64) {
          continue;  // Tag_nodefaults carries no value.
        }
        const auto value = Uleb(attributes, &sub);
        if (!value.ok()) {
          return value.status();
        }
        if (*attribute == 32) {  // Compatibility: integer followed by NTBS.
          const auto text = String(attributes, &sub);
          if (!text.ok()) {
            return text.status();
          }
        }
        if (*attribute == 6 || *attribute == 9 || *attribute == 10 ||
            *attribute == 12 || *attribute == 28) {
          if (!seen.insert(*attribute).second) {
            return absl::DataLossError("Duplicate ARM execution attribute");
          }
          switch (*attribute) {
            case 6:
              result.cpu_arch = *value;
              break;
            case 9:
              result.thumb_isa = *value;
              break;
            case 10:
              result.fp_arch = *value;
              break;
            case 12:
              result.simd_arch = *value;
              break;
            case 28:
              result.vfp_args = *value;
              break;
          }
        }
      }
    }
  }
  return result;
}
}  // namespace symbian::analysis
