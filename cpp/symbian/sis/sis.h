#ifndef SYMBIAN_SIS_SIS_H_
#define SYMBIAN_SIS_SIS_H_

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

#include <absl/status/statusor.h>

namespace symbian::sis {

struct PackageOptions {
  uint32_t uid = 0;
  std::string name;
  std::string vendor;
  std::string executable_name;
  std::array<int32_t, 3> version{1, 0, 0};
};

struct PackageInfo {
  PackageOptions options;
  uint32_t executable_uid = 0;
  uint32_t executable_size = 0;
  std::string target;
};

// Deterministic unsigned SISX, English, one import-free experimental E32 EXE,
// ordinary installation to !:\sys\bin. No scripts, dependencies or signature.
// Printable ASCII metadata, experimental UID range and <=16 MiB payload only.
// This is a format experiment, not an authorization to install on hardware.
absl::StatusOr<std::string> BuildPackage(std::string_view executable,
                                         const PackageOptions& options);

// Bounded inspection of exactly the canonical profile above. Verifies UID,
// controller/data CRCs, SHA-1 payload hash and native E32 checks. Other SIS
// profiles return Unimplemented; this is not a general SIS validity verdict.
absl::StatusOr<PackageInfo> InspectPackage(std::string_view bytes);

}  // namespace symbian::sis

#endif  // SYMBIAN_SIS_SIS_H_
