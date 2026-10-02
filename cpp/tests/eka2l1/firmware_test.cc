// SPDX-License-Identifier: GPL-3.0-or-later
#include "symbian/emulator/firmware.h"

#include <filesystem>
#include <fstream>

#include <unistd.h>

#include "config/config.h"
#include "gtest/gtest.h"
#include "system/devices.h"
#include "system/installation/rpkg.h"

namespace symbian::emulator {
TEST(FirmwareRpkgTest,
     TraversalOversizedNamesAndTruncationFailBeforePublication) {
  char temporary[] = "/tmp/symbian-rpkg-XXXXXX";
  ASSERT_NE(mkdtemp(temporary), nullptr);
  const std::filesystem::path directory(temporary);
  eka2l1::config::state config;
  config.storage = directory.string();
  eka2l1::device_manager manager(&config);
  for (int variant = 0; variant != 3; ++variant) {
    const auto path = directory / "test.rpkg";
    std::ofstream output(path, std::ios::binary);
    const auto write = [&output](auto value) {
      output.write(reinterpret_cast<const char*>(&value), sizeof(value));
    };
    for (std::uint32_t magic : {'R', 'P', 'K', 'G'}) {
      write(magic);
    }
    write(std::uint8_t{1});
    write(std::uint8_t{0});
    write(std::uint16_t{0});
    write(std::uint32_t{1});
    write(std::uint64_t{0});
    write(std::uint64_t{0});
    const std::u16string name = u"Z:\\..\\..\\escape";
    write(variant == 1 ? std::uint64_t{1ULL << 40}
                       : std::uint64_t{name.size()});
    output.write(reinterpret_cast<const char*>(name.data()), name.size() * 2);
    write(variant == 2 ? std::uint64_t{100} : std::uint64_t{1});
    output.put('!');
    output.close();
    std::string code;
    EXPECT_EQ(eka2l1::loader::install_rpkg(&manager, path.string(),
                                           (directory / "z").string(), code,
                                           true, nullptr, nullptr),
              eka2l1::device_installation_rpkg_corrupt);
    EXPECT_EQ(manager.total(), 0);
    EXPECT_FALSE(std::filesystem::exists(directory / "escape"));
  }
  std::filesystem::remove_all(directory);
}

TEST(FirmwareArchiveTest, RejectsTraversalAndHostPaths) {
  for (const auto* path : {"../escape", "data/../escape", "/absolute",
                           "C:/host", "data\\escape"}) {
    EXPECT_EQ(ValidateArchive({{path, 0, false}}).code(),
              absl::StatusCode::kInvalidArgument)
        << path;
  }
}

TEST(FirmwareArchiveTest, RejectsCaseCollisionsAndMultipleDevices) {
  EXPECT_EQ(
      ValidateArchive({{"Z/file", 1, false}, {"z/file", 1, false}}).code(),
      absl::StatusCode::kInvalidArgument);
  EXPECT_EQ(
      ValidateArchive({{"one.rom", 1, false}, {"two.rom", 1, false}}).code(),
      absl::StatusCode::kInvalidArgument);
  EXPECT_EQ(
      ValidateArchive({{"data/drives/z/rm-1/sys/bin/euser.dll", 1, false},
                       {"data/drives/z/rm-2/sys/bin/euser.dll", 1, false}})
          .code(),
      absl::StatusCode::kInvalidArgument);
}

TEST(FirmwareArchiveTest, RejectsUnboundedExpansion) {
  EXPECT_EQ(ValidateArchive({{"file", 9ULL << 30, false}}).code(),
            absl::StatusCode::kResourceExhausted);
}

TEST(FirmwareArchiveTest, AcceptsOriginalEmulatorLayouts) {
  EXPECT_TRUE(ValidateArchive(
                  {{"dump/data/roms/rm-1/SYM.ROM", 512, false},
                   {"dump/data/drives/z/rm-1/sys/bin/euser.dll", 100, false}})
                  .ok());
  EXPECT_TRUE(ValidateArchive({{"dump/device.rom", 512, false},
                               {"dump/device.rpkg", 100, false}})
                  .ok());
  EXPECT_TRUE(
      ValidateArchive(
          {{"data/roms/rm-346/SYM.rom", 512, false},
           {"data/drives/z/rm-346/system/data/quickoffice/registration.rom", 0,
            false}})
          .ok());
}
}  // namespace symbian::emulator
