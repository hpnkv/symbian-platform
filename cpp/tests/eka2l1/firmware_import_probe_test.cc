// SPDX-License-Identifier: GPL-3.0-or-later
// Imports explicitly supplied files into a new emulator-only root. No phone
// transport, hardware recovery, or flashing facility is called.

#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include <absl/base/nullability.h>
#include <config/config.h>
#include <gtest/gtest.h>
#include <system/devices.h>
#include <system/installation/firmware.h>

namespace {

TEST(FirmwareImportProbeTest, ImportsProvidedVplIntoNewDisposableEmulatorRoot) {
  const char* absl_nullable source = std::getenv("SYMBIAN_FIRMWARE_VPL");
  const char* absl_nullable destination =
      std::getenv("SYMBIAN_EMULATOR_IMPORT_ROOT");
  if (!source || !destination) {
    GTEST_SKIP() << "Supply a preserved VPL and a new emulator output root";
  }
  namespace fs = std::filesystem;
  std::error_code error;
  const fs::path vpl = fs::canonical(source, error);
  ASSERT_FALSE(error) << error.message();
  ASSERT_TRUE(fs::is_regular_file(vpl, error));
  ASSERT_FALSE(error);
  const fs::path root(destination);
  ASSERT_TRUE(root.is_absolute());
  ASSERT_FALSE(fs::exists(root, error)) << "Use a fresh disposable root";
  ASSERT_FALSE(error);
  ASSERT_TRUE(fs::create_directories(root / "data/roms", error));
  ASSERT_FALSE(error) << error.message();
  eka2l1::config::state config;
  config.storage = (root / "data").string();
  eka2l1::device_manager devices(&config);
  const auto result = eka2l1::install_firmware(
      &devices, vpl.string(), config.storage, (root / "data/roms").string(),
      true,
      [](const std::vector<std::string>& variants) {
        EXPECT_EQ(variants.size(), 1);
        return variants.size() == 1 ? 0 : -1;
      },
      nullptr, [] { return false; });
  ASSERT_EQ(result, eka2l1::device_installation_none);
  ASSERT_EQ(devices.total(), 1);
  const auto* absl_nonnull device = devices.lastest();
  ASSERT_NE(device, nullptr);
  EXPECT_TRUE(device->isolated_drives);
  devices.save_devices();
  RecordProperty("firmware_code", device->firmware_code);
  RecordProperty("model", device->model);
  RecordProperty("manufacturer", device->manufacturer);
  RecordProperty("epoc_version_enum",
                 std::to_string(static_cast<int>(device->ver)));
  RecordProperty("emulator_root", root.string());
  RecordProperty("os_boot_verified", "false");
  RecordProperty("physical_device_match_verified", "false");
  EXPECT_TRUE(fs::is_regular_file(root / "data/devices.yml", error));
  EXPECT_FALSE(error);
}

}  // namespace
