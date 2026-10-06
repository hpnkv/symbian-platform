// SPDX-License-Identifier: GPL-3.0-or-later
// Installs and removes a trusted, prevalidated maintained SISX fixture using
// EKA2L1's unchanged parser, package manager, registry and kernel. No ROM.

#include <fstream>
#include <iterator>

#include <absl/base/nullability.h>
#include <package/manager.h>

#include "process_environment.h"

namespace {

namespace fs = std::filesystem;

std::string ReadFile(const fs::path& path) {
  std::ifstream file(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(file),
          std::istreambuf_iterator<char>()};
}

class PackageProbeTest : public symbian::testing::ProcessEnvironment {
 protected:
  void Populate(const fs::path& artifact) override {
    const char* absl_nullable package = std::getenv("SYMBIAN_SIS_TEST_PACKAGE");
    ASSERT_NE(package, nullptr);
    std::error_code error;
    const fs::path source = fs::absolute(package, error);
    ASSERT_FALSE(error);
    fs::copy_file(source, directory_ / "probe.sis", error);
    ASSERT_FALSE(error) << error.message();
    original_image_ = ReadFile(artifact);
    ASSERT_FALSE(original_image_.empty());
  }

  uint32_t DriveAttributes() override { return io_attrib_internal; }

  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(ProcessEnvironment::SetUp());
    packages_ = std::make_unique<eka2l1::manager::packages>(io_.get(), &config_,
                                                            drive_c);
    ASSERT_FALSE(io_->exist(u"C:\\sys\\bin\\probe.exe"));
    ASSERT_EQ(packages_->package(0xe0000809, 0), nullptr);
  }

  void TearDown() override {
    packages_.reset();
    ProcessEnvironment::TearDown();
  }

  void Install() {
    ASSERT_EQ(packages_->install_package((directory_ / "probe.sis").u16string(),
                                         drive_c, nullptr, nullptr, true),
              eka2l1::package::installation_result_success);
    ASSERT_TRUE(io_->exist(u"C:\\sys\\bin\\probe.exe"));
    EXPECT_EQ(ReadFile(directory_ / "sys/bin/probe.exe"), original_image_);
    auto* absl_nonnull package = packages_->package(0xe0000809, 0);
    ASSERT_NE(package, nullptr);
    EXPECT_EQ(package->package_name, u"Symbian E32 Probe");
    EXPECT_EQ(package->vendor_name, u"Symbian research");
    EXPECT_EQ(package->version.major, 1);
    EXPECT_EQ(package->version.minor, 0);
    EXPECT_EQ(package->version.build, 0);
    ASSERT_EQ(package->file_descriptions.size(), 1);
    EXPECT_EQ(package->file_descriptions.front().sid, 0xe0000808);
    // Independent Python hashlib reference for additional maintained probes.
    // The original probe retains its recorded historical baseline by default.
    std::string expected_hash(
        "\xf4\x64\x49\x0f\xdf\x04\xc8\x0d\xf2\xe7"
        "\x78\xf6\x53\x26\x18\x9e\x9e\x2b\x38\xd8",
        20);
    if (const char* absl_nullable reference =
            std::getenv("SYMBIAN_E32_TEST_HASH")) {
      expected_hash = ReadFile(reference);
      ASSERT_EQ(expected_hash.size(), 20);
    }
    const auto& hash = package->file_descriptions.front().hash;
    EXPECT_EQ(hash.algorithm, 1);
    EXPECT_EQ(std::string(hash.data.begin(), hash.data.end()), expected_hash);
    EXPECT_EQ(package->file_descriptions.front().target,
              u"C:\\sys\\bin\\probe.exe");
    EXPECT_TRUE(io_->exist(
        u"C:\\sys\\install\\sisregistry\\e0000809\\00000000_0000.ctl"));
    RecordProperty("package_uid", "3758098441");
    RecordProperty("installed_bytes_match", "true");
  }

  void Uninstall() {
    auto* absl_nonnull package = packages_->package(0xe0000809, 0);
    ASSERT_NE(package, nullptr);
    ASSERT_TRUE(packages_->uninstall_package(*package));
    EXPECT_FALSE(io_->exist(u"C:\\sys\\bin\\probe.exe"));
    EXPECT_EQ(packages_->package(0xe0000809, 0), nullptr);
    EXPECT_EQ(kernel_->spawn_new_process(u"C:\\sys\\bin\\probe.exe", u""),
              nullptr);
  }

  std::unique_ptr<eka2l1::manager::packages> packages_;
  std::string original_image_;
};

TEST_P(PackageProbeTest, InstallsUnchangedExecutableAndRunsThroughKernel) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(Execute(false));
}

TEST_P(PackageProbeTest, RegistrySurvivesReloadAndUninstallRemovesExecutable) {
  ASSERT_NO_FATAL_FAILURE(Install());
  packages_.reset();
  packages_ =
      std::make_unique<eka2l1::manager::packages>(io_.get(), &config_, drive_c);
  packages_->load_registries();
  ASSERT_NE(packages_->package(0xe0000809, 0), nullptr);
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_P(PackageProbeTest, CanReinstallAfterUninstallAndRunAgain) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(Execute(true));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(Execute(false));
  EXPECT_EQ(exits_, 2);
  RecordProperty("completed_processes", "2");
}

INSTANTIATE_TEST_SUITE_P(Backends, PackageProbeTest,
                         ::testing::Values(arm_emulator_type::dyncom,
                                           arm_emulator_type::dynarmic));

}  // namespace
