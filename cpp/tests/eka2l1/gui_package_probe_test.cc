// SPDX-License-Identifier: GPL-3.0-or-later
// Exercises real installer/registry operations on the maintained GUI package.
// Supplies no ROM, system DLLs or Window Server. No CPU instructions execute.
#include <cstring>
#include <fstream>
#include <iterator>

#include <common/buffer.h>
#include <loader/e32img.h>
#include <package/manager.h>

#include "process_environment.h"

namespace {

namespace fs = std::filesystem;

std::string ReadFile(const fs::path& path) {
  std::ifstream file(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(file),
          std::istreambuf_iterator<char>()};
}

class GuiPackageProbeTest : public symbian::testing::ProcessEnvironment {
 protected:
  void Populate(const fs::path& artifact) override {
    const char* package = std::getenv("SYMBIAN_SIS_TEST_PACKAGE");
    const char* hash = std::getenv("SYMBIAN_E32_TEST_HASH");
    ASSERT_NE(package, nullptr);
    ASSERT_NE(hash, nullptr);
    std::error_code error;
    fs::copy_file(fs::absolute(package, error), directory_ / "gui.sis", error);
    ASSERT_FALSE(error) << error.message();
    original_image_ = ReadFile(artifact);
    expected_hash_ = ReadFile(hash);
    ASSERT_FALSE(original_image_.empty());
    ASSERT_EQ(expected_hash_.size(), 20);
  }

  uint32_t DriveAttributes() override { return io_attrib_internal; }

  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(ProcessEnvironment::SetUp());
    packages_ = std::make_unique<eka2l1::manager::packages>(io_.get(), &config_,
                                                            drive_c);
    ASSERT_FALSE(io_->exist(u"C:\\sys\\bin\\gui_app.exe"));
    ASSERT_EQ(packages_->package(0xe0000812, 0), nullptr);
    RecordProperty("emulator_os_profile", "epoc10");
    RecordProperty("cpu_backend", GetParam() == arm_emulator_type::dyncom
                                      ? "dyncom"
                                      : "dynarmic");
    RecordProperty("cpu_instructions_executed", "0");
  }

  void TearDown() override {
    packages_.reset();
    ProcessEnvironment::TearDown();
  }

  void CheckInstalled() {
    ASSERT_TRUE(io_->exist(u"C:\\sys\\bin\\gui_app.exe"));
    EXPECT_EQ(ReadFile(directory_ / "sys/bin/gui_app.exe"), original_image_);
    auto* package = packages_->package(0xe0000812, 0);
    ASSERT_NE(package, nullptr);
    EXPECT_EQ(package->package_name, u"Symbian GUI Counter");
    EXPECT_EQ(package->vendor_name, u"Symbian research");
    EXPECT_EQ(package->version.major, 1);
    EXPECT_EQ(package->version.minor, 0);
    EXPECT_EQ(package->version.build, 0);
    ASSERT_EQ(package->file_descriptions.size(), 1);
    const auto& description = package->file_descriptions.front();
    EXPECT_EQ(description.sid, 0xe0000811);
    EXPECT_EQ(description.target, u"C:\\sys\\bin\\gui_app.exe");
    EXPECT_EQ(description.hash.algorithm, 1);
    EXPECT_EQ(
        std::string(description.hash.data.begin(), description.hash.data.end()),
        expected_hash_);
    EXPECT_TRUE(io_->exist(
        u"C:\\sys\\install\\sisregistry\\e0000812\\00000000_0000.ctl"));
    EXPECT_FALSE(io_->exist(u"C:\\sys\\bin\\euser.dll"));
    EXPECT_FALSE(io_->exist(u"C:\\sys\\bin\\ws32.dll"));
    EXPECT_EQ(exits_, 0);
  }

  void Install() {
    ASSERT_EQ(packages_->install_package((directory_ / "gui.sis").u16string(),
                                         drive_c, nullptr, nullptr, true),
              eka2l1::package::installation_result_success);
    ASSERT_NO_FATAL_FAILURE(CheckInstalled());
  }

  void Uninstall() {
    auto* package = packages_->package(0xe0000812, 0);
    ASSERT_NE(package, nullptr);
    ASSERT_TRUE(packages_->uninstall_package(*package));
    EXPECT_FALSE(io_->exist(u"C:\\sys\\bin\\gui_app.exe"));
    EXPECT_EQ(packages_->package(0xe0000812, 0), nullptr);
    EXPECT_EQ(exits_, 0);
  }

  std::unique_ptr<eka2l1::manager::packages> packages_;
  std::string original_image_;
  std::string expected_hash_;
};

TEST_P(GuiPackageProbeTest, InstallsExactImageAndOriginalRegistryMetadata) {
  ASSERT_NO_FATAL_FAILURE(Install());
}

TEST_P(GuiPackageProbeTest, ReloadsCompleteRegistryAndRemovesInstalledFile) {
  ASSERT_NO_FATAL_FAILURE(Install());
  packages_.reset();
  packages_ =
      std::make_unique<eka2l1::manager::packages>(io_.get(), &config_, drive_c);
  packages_->load_registries();
  ASSERT_NO_FATAL_FAILURE(CheckInstalled());
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_P(GuiPackageProbeTest, ReinstallsExactImageAfterUninstall) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(Uninstall());
  ASSERT_NO_FATAL_FAILURE(Install());
}

TEST_P(GuiPackageProbeTest, MissingSystemLibrariesLeaveUnresolvedImportSlots) {
  ASSERT_NO_FATAL_FAILURE(Install());
  eka2l1::common::ro_buf_stream buffer(
      reinterpret_cast<uint8_t*>(original_image_.data()),
      original_image_.size());
  const auto image = eka2l1::loader::parse_e32img(&buffer, true);
  ASSERT_TRUE(image.has_value());
  ASSERT_EQ(image->import_section.imports.size(), 2);
  auto* process =
      kernel_->spawn_new_process(u"C:\\sys\\bin\\gui_app.exe", u"", 0xe0000811);
  // Upstream ignores failed import fixups. Check the unresolved mapping;
  // creating this process does not mean it can execute its system calls.
  ASSERT_NE(process, nullptr);
  size_t unresolved = 0;
  for (const auto& block : image->import_section.imports) {
    for (const uint32_t offset : block.ordinals) {
      ASSERT_LE(offset + 4, image->header.code_size);
      uint32_t original;
      std::memcpy(&original,
                  original_image_.data() + image->header.code_offset + offset,
                  sizeof(original));
      ASSERT_GE(original, 1);
      ASSERT_LE(original, 65535);
      const auto* mapped =
          static_cast<const uint32_t*>(process->get_ptr_on_addr_space(
              process->get_entry_point_address() + offset));
      ASSERT_NE(mapped, nullptr);
      EXPECT_EQ(*mapped, original);
      ++unresolved;
    }
  }
  EXPECT_EQ(unresolved, 37);
  RecordProperty("unresolved_import_slots", std::to_string(unresolved));
  RecordProperty("process_created_without_system_libraries", "true");
  EXPECT_EQ(exits_, 0);
  EXPECT_FALSE(exception_);
}

INSTANTIATE_TEST_SUITE_P(Backends, GuiPackageProbeTest,
                         ::testing::Values(arm_emulator_type::dyncom,
                                           arm_emulator_type::dynarmic));

}  // namespace
