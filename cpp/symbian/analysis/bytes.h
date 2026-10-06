#ifndef SYMBIAN_ANALYSIS_BYTES_H_
#define SYMBIAN_ANALYSIS_BYTES_H_

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace symbian::analysis::internal {

// Callers must bound the range before reading or writing.
inline uint16_t Read16(std::string_view bytes, size_t offset) {
  return static_cast<uint16_t>(static_cast<uint8_t>(bytes[offset])) |
         static_cast<uint16_t>(static_cast<uint8_t>(bytes[offset + 1]) << 8);
}

inline uint32_t Read32(std::string_view bytes, size_t offset) {
  uint32_t value = 0;
  for (size_t i = 0; i < 4; ++i) {
    value |= static_cast<uint32_t>(static_cast<uint8_t>(bytes[offset + i]))
             << (i * 8);
  }
  return value;
}

inline void Put16(std::span<char> bytes, size_t offset, uint16_t value) {
  bytes[offset] = static_cast<char>(value & 0xff);
  bytes[offset + 1] = static_cast<char>(value >> 8);
}

inline void Put32(std::span<char> bytes, size_t offset, uint32_t value) {
  for (size_t i = 0; i < 4; ++i) {
    bytes[offset + i] = static_cast<char>((value >> (i * 8)) & 0xff);
  }
}

inline bool Within(uint64_t size, uint64_t offset, uint64_t length) {
  return offset <= size && length <= size - offset;
}

}  // namespace symbian::analysis::internal

#endif  // SYMBIAN_ANALYSIS_BYTES_H_
