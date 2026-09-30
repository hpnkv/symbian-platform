// SPDX-License-Identifier: GPL-3.0-or-later
// Trusted, opt-in ROM analysis using the unchanged upstream ROM parsers.
// Firmware and outputs remain private; this performs no guest execution.
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <vector>

#include <common/buffer.h>
#include <loader/rom.h>
#include <loader/romimage.h>

#include "process_environment.h"

namespace {

class RomAbiProbeTest : public symbian::testing::ProcessEnvironment {
 protected:
  void SetUp() override {
    const char* rom = std::getenv("SYMBIAN_ROM_ABI_ROOT");
    const char* output = std::getenv("SYMBIAN_ROM_ABI_OUTPUT");
    if (!rom || !output) {
      GTEST_SKIP() << "Supply a preserved imported root and a fresh output";
    }
    input_ = rom;
    output_ = output;
    ASSERT_TRUE(input_.is_absolute());
    ASSERT_TRUE(output_.is_absolute());
    std::error_code error;
    input_ = std::filesystem::canonical(input_, error);
    ASSERT_FALSE(error);
    output_ = std::filesystem::weakly_canonical(output_, error);
    ASSERT_FALSE(error);
    ASSERT_NE(std::mismatch(input_.begin(), input_.end(), output_.begin(),
                            output_.end())
                  .first,
              input_.end())
        << "Output cannot be inside the preserved input";
    ASSERT_NE(std::mismatch(output_.begin(), output_.end(), input_.begin(),
                            input_.end())
                  .first,
              output_.end())
        << "Output cannot contain the preserved input";
    ASSERT_FALSE(std::filesystem::exists(output_, error));
    ASSERT_FALSE(error);
    ASSERT_NO_FATAL_FAILURE(ProcessEnvironment::SetUp());
  }

  std::filesystem::path input_;
  std::filesystem::path output_;
};

TEST_P(RomAbiProbeTest, ReadsActualEuserExportsAndRetainsCodeForDisassembly) {
  const auto source = input_ / "data/roms/rm-807/SYM.ROM";
  std::ifstream rom_file(source, std::ios::binary);
  ASSERT_TRUE(rom_file.is_open());
  std::vector<std::uint8_t> rom_bytes(
      (std::istreambuf_iterator<char>(rom_file)),
      std::istreambuf_iterator<char>());
  ASSERT_LT(rom_bytes.size(), 64U * 1024 * 1024);
  eka2l1::common::ro_buf_stream rom_stream(rom_bytes.data(), rom_bytes.size());
  const auto rom = eka2l1::loader::load_rom(&rom_stream);
  ASSERT_TRUE(rom.has_value());
  EXPECT_EQ(rom->header.rom_base, 0x80000000U);

  // Map a private copy: upstream ROM mappings use mutable guest memory.
  const auto private_rom = directory_ / "SYM.ROM";
  std::error_code error;
  ASSERT_TRUE(std::filesystem::copy_file(source, private_rom, error));
  ASSERT_FALSE(error);
  ASSERT_TRUE(kernel_->map_rom(rom->header.rom_base, private_rom.string()));

  const auto euser_path = input_ / "data/drives/z/rm-807/sys/bin/euser.dll";
  std::ifstream image_file(euser_path, std::ios::binary);
  ASSERT_TRUE(image_file.is_open());
  std::vector<std::uint8_t> image_bytes(
      (std::istreambuf_iterator<char>(image_file)),
      std::istreambuf_iterator<char>());
  ASSERT_LT(image_bytes.size(), 1024U * 1024);
  eka2l1::common::ro_buf_stream image_stream(image_bytes.data(),
                                             image_bytes.size());
  const auto image = eka2l1::loader::parse_romimg(&image_stream, memory_.get(),
                                                  epocver::epoc10);
  ASSERT_TRUE(image.has_value());
  EXPECT_EQ(image->header.uid3, 0x100039e5U);
  EXPECT_EQ(image->header.code_address, 0x804bcce8U);
  ASSERT_GT(image->header.code_size, 0);
  ASSERT_LT(image->header.code_size, 1024 * 1024);
  ASSERT_GT(image->exports.size(), 2454);
  ASSERT_LT(image->exports.size(), 4096);
  std::vector<std::uint8_t> code(image->header.code_size);
  ASSERT_TRUE(
      memory_->read(image->header.code_address, code.data(), code.size()));

  ASSERT_TRUE(std::filesystem::create_directories(output_, error));
  ASSERT_FALSE(error);
  std::ofstream binary(output_ / "euser-code.bin", std::ios::binary);
  ASSERT_TRUE(binary.is_open());
  binary.write(reinterpret_cast<const char*>(code.data()), code.size());
  binary.close();
  ASSERT_TRUE(binary.good());
  std::ofstream exports(output_ / "exports.tsv");
  ASSERT_TRUE(exports.is_open());
  for (std::size_t i = 0; i < image->exports.size(); ++i) {
    exports << i + 1 << '\t' << "0x" << std::hex << image->exports[i]
            << std::dec << '\n';
  }
  exports.close();
  ASSERT_TRUE(exports.good());
  RecordProperty("code_address", std::to_string(image->header.code_address));
  RecordProperty("code_size", std::to_string(image->header.code_size));
  RecordProperty("exports", std::to_string(image->exports.size()));
  RecordProperty("entry_point", std::to_string(image->header.entry_point));
  RecordProperty("rom_primary_file", std::to_string(rom->header.primary_file));
  RecordProperty("image_version", std::to_string(image->header.major) + "." +
                                      std::to_string(image->header.minor) +
                                      "." +
                                      std::to_string(image->header.build));
  RecordProperty("guest_instructions_executed", "0");
}

INSTANTIATE_TEST_SUITE_P(Dynarmic, RomAbiProbeTest,
                         ::testing::Values(arm_emulator_type::dynarmic));

}  // namespace
