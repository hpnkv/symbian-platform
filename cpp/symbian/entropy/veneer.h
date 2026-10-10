// SPDX-License-Identifier: Apache-2.0
#ifndef SYMBIAN_ENTROPY_VENEER_H_
#define SYMBIAN_ENTROPY_VENEER_H_

#include <cstdint>
#include <span>

namespace symbian::entropy {

// Recognize the original EUSER Thumb wrappers, including the leaving wrapper's
// subsequent BL to LeaveIfError. Returns the ARM-state secure RNG veneer, or
// zero for an unsupported wrapper. Callers must independently validate that
// both wrapper spans and the resulting veneer belong to readable native ROM.
inline std::uintptr_t SecureRandomVeneer(std::uintptr_t address,
                                         std::span<const std::uint16_t> code,
                                         bool leaving) {
  if (const std::size_t words = leaving ? 6 : 4;
      (address & 1) == 0 || code.size() != words || code[0] != 0xb510 ||
      (code[1] & 0xf800) != 0xf000 || (code[2] & 0xf801) != 0xe800 ||
      code[words - 1] != 0xbd10) {
    return 0;
  }
  if (leaving &&
      ((code[3] & 0xf800) != 0xf000 || (code[4] & 0xf800) != 0xf800)) {
    return 0;
  }
  const std::uint32_t raw =
      ((code[1] & 0x7ffu) << 12) | ((code[2] & 0x7ffu) << 1);
  const std::int64_t displacement =
      (raw & 0x400000u) ? std::int64_t{raw} - 0x800000 : std::int64_t{raw};
  const std::int64_t target =
      static_cast<std::int64_t>(((address & ~std::uintptr_t{1}) + 6) &
                                ~std::uintptr_t{3}) +
      displacement;
  if (target <= 0 || target > UINT32_MAX || (target & 3) != 0) {
    return 0;
  }
  return static_cast<std::uintptr_t>(target);
}

inline bool IsSecureRandomVeneer(std::span<const std::uint32_t> code) {
  // ARM SVC (OS-selected executive number) followed by BX LR. The semantics
  // come from the two EUSER export contracts, not a device/syscall table.
  return code.size() == 2 && (code[0] & 0xff000000u) == 0xef000000u &&
         code[1] == 0xe12fff1eu;
}

}  // namespace symbian::entropy
#endif  // SYMBIAN_ENTROPY_VENEER_H_
