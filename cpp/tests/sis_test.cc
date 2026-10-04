#include "symbian/sis/sis.h"

#include <cstddef>
#include <string>

#include <absl/status/status.h>
#include <gtest/gtest.h>

#include "e32_fixture.h"
#include "symbian/analysis/bytes.h"
#include "symbian/analysis/checksum.h"
#include "symbian/e32/e32.h"

namespace symbian::sis {
namespace {

using analysis::internal::Crc16;
using analysis::internal::Put16;
using analysis::internal::Put32;
using analysis::internal::Read32;
using analysis::internal::UidChecksum;

std::string Resource(uint32_t uid2, uint32_t uid3) {
  std::string bytes(24, '\0');
  Put32(bytes, 0, 0x101f4a6b);
  Put32(bytes, 4, uid2);
  Put32(bytes, 8, uid3);
  Put32(bytes, 12, UidChecksum(bytes));
  return bytes;
}

PackageOptions Options() {
  return {0xe0000809, "Probe", "Symbian research", "probe.exe", {1, 2, 3}};
}

class SisTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const auto image =
        e32::ConvertPicExecutable(testing::Executable(), 0xe0000808);
    ASSERT_TRUE(image.ok()) << image.status();
    image_ = *image;
    const auto package = BuildPackage(image_, Options());
    ASSERT_TRUE(package.ok()) << package.status();
    package_ = *package;
  }

  std::string image_;
  std::string package_;
};

TEST_F(SisTest,
       DeterministicOrdinaryPackageHasSeparatePackageAndExecutableUids) {
  EXPECT_EQ(package_, *BuildPackage(image_, Options()));
  const auto info = InspectPackage(package_);
  ASSERT_TRUE(info.ok()) << info.status();
  EXPECT_EQ(info->options.uid, 0xe0000809);
  EXPECT_EQ(info->options.version, (std::array<int32_t, 3>{1, 2, 3}));
  EXPECT_EQ(info->executable_uid, 0xe0000808);
  EXPECT_EQ(info->executable_size, image_.size());
  EXPECT_EQ(info->target, "!:\\sys\\bin\\probe.exe");
  EXPECT_EQ(info->options.name, "Probe");
  EXPECT_EQ(info->options.vendor, "Symbian research");
}

TEST_F(SisTest, RegisteredPackageOwnsThreeVerifiedFiles) {
  const std::string registration = Resource(0x101f8021, 0xe0000808);
  const std::string caption = Resource(0, 0);
  const auto package =
      BuildRegisteredPackage(image_, registration, caption, Options());
  ASSERT_TRUE(package.ok()) << package.status();
  const auto info = InspectPackage(*package);
  ASSERT_TRUE(info.ok()) << info.status();
  ASSERT_EQ(info->files.size(), 3);
  EXPECT_TRUE(info->application_registered);
  EXPECT_EQ(info->files[0].target, "!:\\sys\\bin\\probe.exe");
  EXPECT_EQ(info->files[1].target,
            "!:\\private\\10003a3f\\import\\apps\\probe_reg.rsc");
  EXPECT_EQ(info->files[2].target, "!:\\resource\\apps\\probe_loc.rsc");
  EXPECT_EQ(info->files[1].size, registration.size());
  EXPECT_EQ(info->files[2].size, caption.size());
  EXPECT_FALSE(info->files[1].sha1.empty());
  EXPECT_EQ(BuildRegisteredPackage(image_, registration, caption, Options()),
            package);
  std::string wrong = registration;
  Put32(wrong, 8, 0xe0000809);
  Put32(wrong, 12, UidChecksum(wrong));
  EXPECT_EQ(
      BuildRegisteredPackage(image_, wrong, caption, Options()).status().code(),
      absl::StatusCode::kInvalidArgument);
}

TEST_F(SisTest, LocalesAndSvgIconUseCanonicalBoundedAssets) {
  const auto icon = BuildSvgMif(
      R"(<svg xmlns="http://www.w3.org/2000/svg"><circle r="5"/></svg>)");
  ASSERT_TRUE(icon.ok()) << icon.status();
  EXPECT_EQ(Read32(*icon, 0), 0x34232342);
  EXPECT_EQ(Read32(*icon, 4), 2);
  EXPECT_EQ(Read32(*icon, 12), 2);
  EXPECT_EQ(Read32(*icon, 16), 32);
  EXPECT_EQ(Read32(*icon, 32), 0x34232343);
  EXPECT_EQ(Read32(*icon, 48), 1);
  const std::string prefix = "!:\\resource\\apps\\probe";
  std::vector<ApplicationFile> assets = {
      {"!:\\private\\10003a3f\\import\\apps\\probe_reg.rsc",
       Resource(0x101f8021, 0xe0000808)},
      {prefix + "_loc.rsc", Resource(0, 0)},
      {prefix + "_loc.r01", Resource(0, 0)},
      {prefix + "_loc.r02", Resource(0, 0)},
      {prefix + ".mif", *icon},
  };
  const auto package = BuildApplicationPackage(image_, assets, Options());
  ASSERT_TRUE(package.ok()) << package.status();
  const auto info = InspectPackage(*package);
  ASSERT_TRUE(info.ok()) << info.status();
  EXPECT_EQ(info->files.size(), 6);
  EXPECT_TRUE(info->application_registered);
  EXPECT_EQ(info->files.back().target, prefix + ".mif");
  EXPECT_EQ(BuildApplicationPackage(image_, assets, Options()), package);
  std::swap(assets[2], assets[3]);
  EXPECT_EQ(BuildApplicationPackage(image_, assets, Options()).status().code(),
            absl::StatusCode::kInvalidArgument);
  assets[2].target = "!:\\sys\\bin\\unsafe.dll";
  EXPECT_EQ(BuildApplicationPackage(image_, assets, Options()).status().code(),
            absl::StatusCode::kInvalidArgument);
}

TEST_F(SisTest, ProjectCaBundleIsBoundedAndRoundTrips) {
  const std::string prefix = "!:\\resource\\apps\\probe";
  std::vector<ApplicationFile> assets = {
      {"!:\\private\\10003a3f\\import\\apps\\probe_reg.rsc",
       Resource(0x101f8021, 0xe0000808)},
      {prefix + "_loc.rsc", Resource(0, 0)},
      {prefix + "_ca.pem",
       "-----BEGIN CERTIFICATE-----\nQQ==\n-----END CERTIFICATE-----\n"},
  };
  const auto package = BuildApplicationPackage(image_, assets, Options());
  ASSERT_TRUE(package.ok()) << package.status();
  const auto info = InspectPackage(*package);
  ASSERT_TRUE(info.ok()) << info.status();
  ASSERT_EQ(info->files.size(), 4);
  EXPECT_EQ(info->files.back().target, prefix + "_ca.pem");
  assets.back().target = "!:\\resource\\apps\\other_ca.pem";
  EXPECT_EQ(BuildApplicationPackage(image_, assets, Options()).status().code(),
            absl::StatusCode::kInvalidArgument);
  assets.back().target = prefix + "_ca.pem";
  assets.back().bytes.resize(262145, 'x');
  EXPECT_EQ(BuildApplicationPackage(image_, assets, Options()).status().code(),
            absl::StatusCode::kInvalidArgument);
}

TEST_F(SisTest, RejectsEveryTruncationAndSingleByteMutation) {
  for (size_t i = 0; i < package_.size(); ++i) {
    EXPECT_FALSE(InspectPackage(std::string_view(package_).substr(0, i)).ok())
        << i;
    std::string changed = package_;
    changed[i] ^= 1;
    EXPECT_FALSE(InspectPackage(changed).ok()) << i;
  }
  EXPECT_FALSE(InspectPackage(package_ + std::string(1, '\0')).ok());
}

TEST_F(SisTest, DetectsPayloadTamperingEvenWithCorrectDataCrc) {
  std::string changed = package_;
  const size_t offset = changed.find(image_);
  ASSERT_NE(offset, std::string::npos);
  changed[offset + image_.size() - 1] ^= 1;
  // Contents at 16; two padded checksum fields occupy 24 bytes after its header.
  const size_t compressed = 48;
  const size_t data =
      compressed + 8 + ((Read32(changed, compressed + 4) + 3) & ~3U);
  Put16(changed, 44, Crc16(std::string_view(changed).substr(data)));
  const auto result = InspectPackage(changed);
  EXPECT_EQ(result.status().code(), absl::StatusCode::kDataLoss);
  EXPECT_NE(result.status().message().find("SHA-1"), std::string_view::npos);
}

TEST_F(SisTest,
       RejectsCompressionAndInstallationScriptsWithCorrectControllerCrc) {
  // This intentionally supports only the uncompressed ordinary install profile.
  const size_t controller_size = 8 + ((Read32(package_, 52) + 3) & ~3U);
  std::string changed = package_;
  Put32(changed, 56, 1);  // SISCompressed algorithm: deflate.
  Put16(changed, 32,
        Crc16(std::string_view(changed).substr(48, controller_size)));
  EXPECT_EQ(InspectPackage(changed).status().code(),
            absl::StatusCode::kUnimplemented);
  changed = package_;
  // Change the install operation to a run operation, retaining a valid checksum.
  // File description elements omit their type in SISArray: locate their hash.
  const size_t hash = changed.find(std::string("\x19\0\0\0\x20\0\0\0", 8));
  ASSERT_NE(hash, std::string::npos);
  Put32(changed, hash + 40, 2);
  Put16(changed, 32,
        Crc16(std::string_view(changed).substr(48, controller_size)));
  EXPECT_EQ(InspectPackage(changed).status().code(),
            absl::StatusCode::kUnimplemented);
}

TEST_F(SisTest, RejectsHostileFieldLengthsAndTrailingData) {
  for (size_t offset : {size_t{20}, size_t{28}, size_t{40}, size_t{52}}) {
    std::string changed = package_;
    Put32(changed, offset, 0x7ffffffc);
    EXPECT_FALSE(InspectPackage(changed).ok());
  }
  EXPECT_FALSE(InspectPackage(package_ + std::string(4, '\0')).ok());
}

TEST_F(SisTest, RejectsUnsupportedUidMetadataNamesAndVersions) {
  auto options = Options();
  options.uid = 0x20000808;
  EXPECT_EQ(BuildPackage(image_, options).status().code(),
            absl::StatusCode::kInvalidArgument);
  for (const std::string& name :
       {"../probe.exe", "x\\probe.exe", ".exe", "probe.dll", "p:exe.exe"}) {
    options = Options();
    options.executable_name = name;
    EXPECT_FALSE(BuildPackage(image_, options).ok()) << name;
  }
  options = Options();
  options.vendor = "";
  EXPECT_FALSE(BuildPackage(image_, options).ok());
  options.vendor = std::string(129, 'v');
  EXPECT_FALSE(BuildPackage(image_, options).ok());
  options.vendor = "v\n";
  EXPECT_FALSE(BuildPackage(image_, options).ok());
  options = Options();
  options.version[0] = -1;
  EXPECT_FALSE(BuildPackage(image_, options).ok());
  options.version[0] = 32768;
  EXPECT_FALSE(BuildPackage(image_, options).ok());
}

TEST_F(SisTest, RequiresNativeE32ValidationAndBoundedPayload) {
  image_[20] ^= 1;
  EXPECT_EQ(BuildPackage(image_, Options()).status().code(),
            absl::StatusCode::kDataLoss);
  EXPECT_EQ(BuildPackage(std::string(16 * 1024 * 1024 + 1, '\0'), Options())
                .status()
                .code(),
            absl::StatusCode::kResourceExhausted);
}

}  // namespace
}  // namespace symbian::sis
