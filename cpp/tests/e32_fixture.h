#ifndef SYMBIAN_TESTS_E32_FIXTURE_H_
#define SYMBIAN_TESTS_E32_FIXTURE_H_

#include <string>

#include <absl/base/nullability.h>

#include "symbian/analysis/bytes.h"

namespace symbian::testing {
using analysis::internal::Put16;
using analysis::internal::Put32;

// A complete, bounded ET_EXEC with one RX segment, a startup marker and a
// retained R_ARM_CALL to an internal symbol. This is a format fixture only.
inline std::string Executable() {
  std::string bytes(356, '\0');
  bytes.replace(0, 4,
                "\x7f"
                "ELF");
  bytes[4] = bytes[5] = bytes[6] = 1;
  Put16(bytes, 16, 2);
  Put16(bytes, 18, 40);
  Put32(bytes, 20, 1);
  Put32(bytes, 24, 0x8000);
  Put32(bytes, 28, 52);
  Put32(bytes, 32, 116);
  Put32(bytes, 36, 0x05000200);
  Put16(bytes, 40, 52);
  Put16(bytes, 42, 32);
  Put16(bytes, 44, 1);
  Put16(bytes, 46, 40);
  Put16(bytes, 48, 5);
  Put32(bytes, 52, 1);
  Put32(bytes, 56, 84);
  Put32(bytes, 60, 0x8000);
  Put32(bytes, 68, 32);
  Put32(bytes, 72, 32);
  Put32(bytes, 76, 5);
  Put32(bytes, 80, 4);
  Put32(bytes, 84, 0xe31f0000);
  Put32(bytes, 88, 0xe3540001);
  Put32(bytes, 92, 0xea000000);
  Put32(bytes, 100, 0xeb000000);
  // Section 1: text. Section 2: symbols. Section 3: REL. Section 4: strings.
  Put32(bytes, 160, 1);
  Put32(bytes, 164, 6);
  Put32(bytes, 168, 0x8000);
  Put32(bytes, 172, 84);
  Put32(bytes, 176, 32);
  Put32(bytes, 200, 2);
  Put32(bytes, 212, 316);
  Put32(bytes, 216, 32);
  Put32(bytes, 220, 4);
  Put32(bytes, 232, 16);
  Put32(bytes, 240, 9);
  Put32(bytes, 252, 348);
  Put32(bytes, 256, 8);
  Put32(bytes, 260, 2);
  Put32(bytes, 264, 1);
  Put32(bytes, 272, 8);
  Put32(bytes, 280, 3);
  Put32(bytes, 292, 0);
  Put32(bytes, 336, 0x8018);
  Put16(bytes, 346, 1);
  Put32(bytes, 348, 0x8010);
  Put32(bytes, 352, 0x11c);
  return bytes;
}

// Independent code/data addresses, pointers from both mappings to both targets,
// and a Thumb function. File data is 12 bytes; BSS is another 12 bytes.
inline std::string DataExecutable() {
  std::string bytes(580, '\0');
  const auto original = Executable();
  bytes.replace(0, 52, original.substr(0, 52));
  Put32(bytes, 32, 160);
  Put16(bytes, 44, 2);
  Put16(bytes, 48, 8);
  bytes.replace(52, 32, original.substr(52, 32));
  Put32(bytes, 56, 116);
  Put32(bytes, 84, 1);
  Put32(bytes, 88, 148);
  Put32(bytes, 92, 0x20000000);
  Put32(bytes, 100, 12);
  Put32(bytes, 104, 24);
  Put32(bytes, 108, 6);
  Put32(bytes, 112, 4);
  bytes.replace(116, 32, original.substr(84, 32));
  Put32(bytes, 132, 0x20000000);  // Code to initialized data.
  Put32(bytes, 148, 0x8019);      // Data to Thumb code.
  Put32(bytes, 152, 0x20000000);  // Data to data.
  Put32(bytes, 156, 0x2000000c);  // Data to BSS.
  auto section = [&](uint32_t index, uint32_t type, uint32_t flags,
                     uint32_t address, uint32_t offset, uint32_t size,
                     uint32_t link, uint32_t info, uint32_t entry) {
    const uint32_t p = 160 + index * 40;
    Put32(bytes, p + 4, type);
    Put32(bytes, p + 8, flags);
    Put32(bytes, p + 12, address);
    Put32(bytes, p + 16, offset);
    Put32(bytes, p + 20, size);
    Put32(bytes, p + 24, link);
    Put32(bytes, p + 28, info);
    Put32(bytes, p + 36, entry);
  };
  section(1, 1, 6, 0x8000, 116, 32, 0, 0, 0);
  section(2, 2, 0, 0, 480, 64, 7, 0, 16);
  section(3, 9, 0, 0, 544, 8, 2, 1, 8);
  section(4, 1, 3, 0x20000000, 148, 12, 0, 0, 0);
  section(5, 8, 3, 0x2000000c, 160, 12, 0, 0, 0);
  section(6, 9, 0, 0, 552, 24, 2, 4, 8);
  section(7, 3, 0, 0, 576, 4, 0, 0, 0);
  for (uint32_t index = 1; index <= 3; ++index) {
    const uint32_t p = 480 + index * 16;
    Put32(bytes, p + 4,
          index == 1 ? 0x8019 : (index == 2 ? 0x20000000 : 0x2000000c));
    bytes[p + 12] = index == 1 ? 2 : 1;
    Put16(bytes, p + 14, index == 1 ? 1 : (index == 2 ? 4 : 5));
  }
  Put32(bytes, 544, 0x8010);
  Put32(bytes, 548, 0x202);
  for (uint32_t index = 0; index < 3; ++index) {
    Put32(bytes, 552 + index * 8, 0x20000000 + index * 4);
    Put32(bytes, 556 + index * 8, ((index + 1) << 8) | 2);
  }
  return bytes;
}

}  // namespace symbian::testing

#endif  // SYMBIAN_TESTS_E32_FIXTURE_H_
