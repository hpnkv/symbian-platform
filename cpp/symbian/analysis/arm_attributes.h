#ifndef SYMBIAN_ANALYSIS_ARM_ATTRIBUTES_H_
#define SYMBIAN_ANALYSIS_ARM_ATTRIBUTES_H_

#include <cstdint>
#include <string_view>

#include <absl/status/statusor.h>

namespace symbian::analysis {

// Values use the original AEABI Tag_* encodings, not a hardware model guess.
struct ArmAttributes {
  uint32_t cpu_arch = 0;
  uint32_t fp_arch = 0;
  uint32_t simd_arch = 0;
  uint32_t thumb_isa = 0;
  uint32_t vfp_args = 0;
};

// Bounded ARM ELF attribute section reader. Native parsing only; callers apply
// supported SDK/ISA policy. Missing CPU metadata remains zero (unspecified).
absl::StatusOr<ArmAttributes> InspectArmAttributes(std::string_view bytes);

}  // namespace symbian::analysis
#endif  // SYMBIAN_ANALYSIS_ARM_ATTRIBUTES_H_
