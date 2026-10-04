#ifndef SYMBIAN_SIS_SIS_H_
#define SYMBIAN_SIS_SIS_H_

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

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
  std::string executable_sha1;  // Hex digest of the verified embedded payload.
  std::string target;

  struct EmbeddedFile {
    std::string target;
    uint32_t size = 0;
    std::string sha1;
  };

  std::vector<EmbeddedFile> files;
  bool application_registered = false;
};

struct ApplicationFile {
  std::string target;
  std::string bytes;
};

// Deterministic unsigned SISX, English, one validated E32 application EXE,
// ordinary installation to !:\sys\bin. No scripts, dependencies or signature.
// Printable ASCII metadata, experimental UID range and <=16 MiB payload only.
// This application package format does not authorize installation on hardware.
absl::StatusOr<std::string> BuildPackage(std::string_view executable,
                                         const PackageOptions& options);

// The same bounded profile with original rcomp-produced registration and
// localisable-caption resources installed on the executable's drive.
absl::StatusOr<std::string> BuildRegisteredPackage(
    std::string_view executable, std::string_view registration,
    std::string_view caption, const PackageOptions& options);

// Includes the registration resource, locale resources and optional icon.
// Targets use the install-drive placeholder and a fixed application subtree.
absl::StatusOr<std::string> BuildApplicationPackage(
    std::string_view executable, const std::vector<ApplicationFile>& assets,
    const PackageOptions& options);

// Wraps a bounded project SVG in a deterministic, gzip-backed MIF icon.
absl::StatusOr<std::string> BuildSvgMif(std::string_view svg);

// Bounded inspection of exactly the canonical profile above. Verifies UID,
// controller/data CRCs, SHA-1 payload hash and native E32 checks. Other SIS
// profiles return Unimplemented; this is not a general SIS validity verdict.
absl::StatusOr<PackageInfo> InspectPackage(std::string_view bytes);

}  // namespace symbian::sis

#endif  // SYMBIAN_SIS_SIS_H_
