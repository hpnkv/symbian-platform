#ifndef SYMBIAN_ANALYSIS_CHECKSUM_H_
#define SYMBIAN_ANALYSIS_CHECKSUM_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace symbian::analysis::internal {

// Symbian CRC16: polynomial 0x1021, initial zero, no final complement.
inline uint16_t Crc16(std::string_view bytes) {
  uint16_t crc = 0;
  for (const char value : bytes) {
    const auto byte = static_cast<uint8_t>(value);
    crc ^= static_cast<uint16_t>(byte << 8);
    for (int bit = 0; bit < 8; ++bit) {
      crc = static_cast<uint16_t>((crc << 1) ^ ((crc & 0x8000) ? 0x1021 : 0));
    }
  }
  return crc;
}

// Callers supply at least twelve bytes containing three little-endian UIDs.
inline uint32_t UidChecksum(std::string_view bytes) {
  std::array<uint16_t, 2> crc{};
  for (size_t i = 0; i < 12; ++i) {
    uint16_t& value = crc[i % 2];
    value ^= static_cast<uint16_t>(static_cast<uint8_t>(bytes[i]) << 8);
    for (int bit = 0; bit < 8; ++bit) {
      value =
          static_cast<uint16_t>((value << 1) ^ ((value & 0x8000) ? 0x1021 : 0));
    }
  }
  return (static_cast<uint32_t>(crc[1]) << 16) | crc[0];
}

}  // namespace symbian::analysis::internal

#endif  // SYMBIAN_ANALYSIS_CHECKSUM_H_
