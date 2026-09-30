// SPDX-License-Identifier: GPL-3.0-or-later
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <system_error>

#include <common/buffer.h>
#include <kernel/codeseg.h>
#include <kernel/process.h>
#include <loader/e32img.h>

#include "process_environment.h"

namespace {
namespace fs = std::filesystem;

class ImportProbeTest : public symbian::testing::ProcessEnvironment {
 protected:
  void Populate(const fs::path& artifact) override {
    ProcessEnvironment::Populate(artifact);
    const char* dll = std::getenv("SYMBIAN_DLL_TEST_IMAGE");
    ASSERT_NE(dll, nullptr);
    std::error_code error;
    fs::copy_file(dll, directory_ / "sys/bin/probe.dll", error);
    ASSERT_FALSE(error) << error.message();
    std::ifstream stream(artifact, std::ios::binary);
    std::string bytes{std::istreambuf_iterator<char>(stream),
                      std::istreambuf_iterator<char>()};
    eka2l1::common::ro_buf_stream buffer(
        reinterpret_cast<uint8_t*>(bytes.data()), bytes.size());
    const auto image = eka2l1::loader::parse_e32img(&buffer, true);
    ASSERT_TRUE(image.has_value());
    ASSERT_EQ(image->import_section.imports.size(), 1);
    const auto& block = image->import_section.imports[0];
    ASSERT_EQ(block.dll_name, "probe.dll");
    ASSERT_EQ(block.ordinals.size(), 1);
    slot_offset_ = block.ordinals[0];
    ASSERT_LT(slot_offset_ + 4, image->header.code_size);
  }

  void BeforeExecute(eka2l1::kernel::process* process) override {
    eka2l1::kernel::codeseg* dll = nullptr;
    for (const auto& code : kernel_->get_codeseg_list()) {
      if (code->name() == "probe.dll") {
        ASSERT_EQ(dll, nullptr);
        dll = static_cast<eka2l1::kernel::codeseg*>(code.get());
      }
    }
    ASSERT_NE(dll, nullptr);
    EXPECT_EQ(dll->export_count(), 7);
    function_ = dll->lookup(process, 7);
    ASSERT_NE(function_, 0);
    EXPECT_NE(function_ & ~1U, 0x8080U);
    const auto* patched =
        static_cast<const uint32_t*>(process->get_ptr_on_addr_space(
            process->get_entry_point_address() + slot_offset_));
    ASSERT_NE(patched, nullptr);
    EXPECT_EQ(*patched, function_);
    saw_function_ = false;
    RecordProperty("imported_dll", "probe.dll");
    RecordProperty("imported_ordinal", "7");
    RecordProperty("patched_slot", std::to_string(*patched));
  }

  void ObserveStep(uint32_t pc) override {
    saw_function_ |= pc == (function_ & ~1U);
  }

  uint32_t function_ = 0;
  uint32_t slot_offset_ = 0;
  bool saw_function_ = false;
};

TEST_P(ImportProbeTest, ResolvesAndExecutesDllFunction) {
  ASSERT_NO_FATAL_FAILURE(Execute(false));
  EXPECT_TRUE(saw_function_);
}

TEST_P(ImportProbeTest, ChangedInputCrossesDllAndProducesFailureExit) {
  ASSERT_NO_FATAL_FAILURE(Execute(true));
  EXPECT_TRUE(saw_function_);
}

TEST_P(ImportProbeTest, RepeatsImportLoadAfterFailureExit) {
  ASSERT_NO_FATAL_FAILURE(Execute(true));
  EXPECT_TRUE(saw_function_);
  ASSERT_NO_FATAL_FAILURE(Execute(false));
  EXPECT_TRUE(saw_function_);
  EXPECT_EQ(exits_, 2);
}

INSTANTIATE_TEST_SUITE_P(Backends, ImportProbeTest,
                         ::testing::Values(arm_emulator_type::dyncom,
                                           arm_emulator_type::dynarmic));

}  // namespace
