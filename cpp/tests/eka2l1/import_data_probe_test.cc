// SPDX-License-Identifier: GPL-3.0-or-later
// Runs a converted writable-data DLL through EKA2L1's actual import loader.

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

#include <absl/base/nullability.h>
#include <common/buffer.h>
#include <kernel/codeseg.h>
#include <kernel/process.h>
#include <loader/e32img.h>

#include "process_environment.h"

namespace {
namespace fs = std::filesystem;

class ImportDataProbeTest : public symbian::testing::ProcessEnvironment {
 protected:
  void TearDown() override {
    RecordProperty("exit_count_at_teardown", std::to_string(exits_));
    ProcessEnvironment::TearDown();
  }

  void Populate(const fs::path& artifact) override {
    ProcessEnvironment::Populate(artifact);
    const char* absl_nullable source = std::getenv("SYMBIAN_DLL_TEST_IMAGE");
    ASSERT_NE(source, nullptr);
    std::error_code error;
    fs::copy_file(source, directory_ / "sys/bin/probe.dll", error);
    ASSERT_FALSE(error) << error.message();

    std::ifstream stream(source, std::ios::binary);
    std::string bytes{std::istreambuf_iterator<char>(stream),
                      std::istreambuf_iterator<char>()};
    eka2l1::common::ro_buf_stream buffer(
        reinterpret_cast<uint8_t*>(bytes.data()), bytes.size());
    const auto image = eka2l1::loader::parse_e32img(&buffer, true);
    ASSERT_TRUE(image.has_value());
    ASSERT_EQ(image->header.data_size, 4);
    ASSERT_EQ(image->header.bss_size, 4);
    ASSERT_EQ(image->header.export_dir_count, 7);
    ASSERT_GE(image->code_reloc_section.num_relocs, 9);
  }

  void BeforeExecute(eka2l1::kernel::process* absl_nonnull process) override {
    eka2l1::kernel::codeseg* absl_nullable dll = nullptr;
    for (const auto& code : kernel_->get_codeseg_list()) {
      if (code->name() == "probe.dll") {
        ASSERT_EQ(dll, nullptr);
        dll = static_cast<eka2l1::kernel::codeseg*>(code.get());
      }
    }
    ASSERT_NE(dll, nullptr);
    function_ = dll->lookup(process, 7);
    ASSERT_NE(function_, 0);
    const auto data_address = dll->get_data_run_addr(process);
    ASSERT_NE(data_address, 0);
    ASSERT_NE(data_address, 0x20000000U);
    const auto* absl_nonnull data = static_cast<const uint32_t*>(
        process->get_ptr_on_addr_space(data_address));
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data[0], 0x808U);
    EXPECT_EQ(data[1], 0U);
    const auto code_address = dll->get_code_run_addr(process);
    const auto* absl_nonnull got = static_cast<const uint32_t*>(
        process->get_ptr_on_addr_space(code_address + 64));
    ASSERT_NE(got, nullptr);
    EXPECT_EQ(got[0], data_address + 4);
    EXPECT_EQ(got[1], data_address);
    RecordProperty("dll_code_address", std::to_string(code_address));
    RecordProperty("dll_function_address", std::to_string(function_));
    RecordProperty("dll_data_address", std::to_string(data_address));
    RecordProperty("dll_data_initialized", std::to_string(data[0]));
    RecordProperty("dll_bss_zero", std::to_string(data[1]));
    calls_ = 0;
  }

  void ObserveStep(uint32_t pc) override {
    if (pc == (function_ & ~1U)) {
      ++calls_;
    }
  }

  uint32_t function_ = 0;
  size_t calls_ = 0;
};

TEST_P(ImportDataProbeTest, InitializesAndResetsDataForEachProcess) {
  ASSERT_NO_FATAL_FAILURE(Execute(false));
  EXPECT_EQ(calls_, 2);
  ASSERT_NO_FATAL_FAILURE(Execute(false));
  EXPECT_EQ(calls_, 2);
  EXPECT_EQ(exits_, 2);
}

TEST_P(ImportDataProbeTest, ChangedInputFailsAcrossDllBoundary) {
  ASSERT_NO_FATAL_FAILURE(Execute(true));
  EXPECT_EQ(calls_, 2);
  ASSERT_NO_FATAL_FAILURE(Execute(false));
  EXPECT_EQ(calls_, 2);
  EXPECT_EQ(exits_, 2);
}

INSTANTIATE_TEST_SUITE_P(Backends, ImportDataProbeTest,
                         ::testing::Values(arm_emulator_type::dyncom,
                                           arm_emulator_type::dynarmic));

}  // namespace
