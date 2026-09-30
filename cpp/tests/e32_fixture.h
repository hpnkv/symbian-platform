#ifndef SYMBIAN_TESTS_E32_FIXTURE_H_
#define SYMBIAN_TESTS_E32_FIXTURE_H_

#include <string>

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

}  // namespace symbian::testing

#endif  // SYMBIAN_TESTS_E32_FIXTURE_H_
