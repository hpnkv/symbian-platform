#ifndef SYMBIAN_TESTS_EKA2L1_PROCESS_ENVIRONMENT_H_
#define SYMBIAN_TESTS_EKA2L1_PROCESS_ENVIRONMENT_H_

// SPDX-License-Identifier: GPL-3.0-or-later
// Executes an import-free E32 process through EKA2L1's loader and kernel.
// No Belle ROM, Z drive, target DLLs or system services are supplied.

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <absl/base/nullability.h>
#include <config/app_settings.h>
#include <config/config.h>
#include <cpu/arm_factory.h>
#include <gtest/gtest.h>
#include <kernel/kernel.h>
#include <kernel/process.h>
#include <kernel/thread.h>
#include <kernel/timing.h>
#include <mem/mem.h>
#include <vfs/vfs.h>

namespace symbian::testing {

namespace fs = std::filesystem;

class ProcessEnvironment : public ::testing::TestWithParam<arm_emulator_type> {
 protected:
  void SetUp() override {
    const char* absl_nullable input = std::getenv("SYMBIAN_E32_TEST_IMAGE");
    ASSERT_NE(input, nullptr);
    std::error_code error;
    const fs::path artifact = fs::absolute(input, error);
    ASSERT_FALSE(error) << error.message();
    original_directory_ = fs::current_path(error);
    ASSERT_FALSE(error) << error.message();
    const fs::path temp = fs::temp_directory_path(error);
    ASSERT_FALSE(error) << error.message();
    const std::string pattern = (temp / "symbian-process-XXXXXX").string();
    std::vector<char> name(pattern.begin(), pattern.end());
    name.push_back('\0');
    ASSERT_NE(mkdtemp(name.data()), nullptr);
    directory_ = name.data();
    fs::create_directories(directory_ / "sys/bin", error);
    ASSERT_FALSE(error) << error.message();
    ASSERT_NO_FATAL_FAILURE(Populate(artifact));
    fs::current_path(directory_, error);
    ASSERT_FALSE(error) << error.message();

    config_.storage = directory_.string();
    config_.cpu_load_save = false;  // Never block on an empty guest run queue.
    ASSERT_EQ(eka2l1::arm::resolve_emulator_type(GetParam()), GetParam());
    monitor_ = eka2l1::arm::create_exclusive_monitor(GetParam(), 1);
    ASSERT_NE(monitor_, nullptr);
    cpu_ = eka2l1::arm::create_core(monitor_.get(), GetParam());
    ASSERT_NE(cpu_, nullptr);
    memory_ = std::make_unique<eka2l1::memory_system>(
        monitor_.get(), &config_, eka2l1::mem::mem_model_type::flexible, false);
    timer_ = std::make_unique<eka2l1::ntimer>(100000000);
    // Use the upstream timer lifecycle; stop its worker before kernel teardown.
    timer_->reset();
    io_ = std::make_unique<eka2l1::io_system>();
    auto filesystem = eka2l1::create_physical_filesystem(epocver::epoc10, "");
    ASSERT_TRUE(io_->add_filesystem(filesystem).has_value());
    ASSERT_TRUE(io_->mount_physical_path(drive_c, drive_media::physical,
                                         DriveAttributes(),
                                         directory_.u16string()));
    settings_ = std::make_unique<eka2l1::config::app_settings>(&config_);
    kernel_ = std::make_unique<eka2l1::kernel_system>(
        nullptr, timer_.get(), io_.get(), &config_, settings_.get(), nullptr,
        cpu_.get(), nullptr);
    kernel_->install_memory(memory_.get());
    kernel_->set_epoc_version(epocver::epoc10);
    kernel_->register_process_exit_callback([this](auto* absl_nonnull process) {
      ++exits_;
      exit_reason_ = process->get_exit_reason();
      exit_type_ = process->get_exit_type();
    });
    // Preserve the kernel's real SVC handler. Faults are fatal test evidence.
    cpu_->exception_handler = [this](auto, uint32_t address) {
      exception_ = true;
      exception_address_ = address;
      cpu_->stop();
      return false;
    };
  }

  virtual void Populate(const fs::path& artifact) {
    std::error_code error;
    fs::copy_file(artifact, directory_ / "sys/bin/probe.exe", error);
    ASSERT_FALSE(error) << error.message();
  }

  virtual uint32_t DriveAttributes() { return io_attrib_write_protected; }

  virtual void BeforeExecute(eka2l1::kernel::process* absl_nonnull) {}

  virtual void ObserveStep(uint32_t) {}

  void TearDown() override {
    if (timer_) {
      timer_->stop();
    }
    kernel_.reset();
    memory_.reset();
    cpu_.reset();
    monitor_.reset();
    settings_.reset();
    io_.reset();
    timer_.reset();
    std::error_code error;
    if (!original_directory_.empty()) {
      fs::current_path(original_directory_, error);
      EXPECT_FALSE(error) << error.message();
    }
    if (!directory_.empty()) {
      fs::remove_all(directory_, error);
      EXPECT_FALSE(error) << error.message();
    }
  }

  void Execute(bool inject_wrong_input) {
    const size_t previous_exits = exits_;
    injections_ = 0;
    auto* absl_nonnull process =
        kernel_->spawn_new_process(u"C:\\sys\\bin\\probe.exe", u"", 0xe0000808);
    ASSERT_NE(process, nullptr);
    auto* absl_nonnull thread = process->get_primary_thread();
    ASSERT_NE(thread, nullptr);
    // Keep diagnostic objects alive through exit assertions. Kernel wipeout
    // owns their final destruction, including early assertion failures.
    process->increase_access_count();
    thread->increase_access_count();
    ASSERT_TRUE(process->get_mem_model());
    const auto entry = process->get_entry_point_address();
    ASSERT_NO_FATAL_FAILURE(BeforeExecute(process));
    RecordProperty("emulator_os_profile", "epoc10");
    RecordProperty("cpu_backend", GetParam() == arm_emulator_type::dyncom
                                      ? "dyncom"
                                      : "dynarmic");
    RecordProperty("code_load_address", std::to_string(entry));
    EXPECT_NE(entry, 0x8000U);  // Memory model chooses the actual code mapping.
    ASSERT_TRUE(process->run());
    kernel_->reschedule();
    ASSERT_EQ(kernel_->crr_process(), process);
    ASSERT_EQ(kernel_->crr_thread(), thread);
    ASSERT_EQ(cpu_->get_pc(), entry);
    const auto initial_sp = cpu_->get_sp();
    inject_wrong_input_ = inject_wrong_input;
    initial_sp_ = initial_sp;
    auto original_write = cpu_->write_32bit;
    cpu_->write_32bit = [this, original_write](uint32_t address,
                                               uint32_t* absl_nonnull value) {
      if (inject_wrong_input_ && *value == 16 && address < initial_sp_ &&
          initial_sp_ - address <= 256) {
        ++injections_;
        uint32_t changed = 17;
        return original_write(address, &changed);
      }
      return original_write(address, value);
    };
    bool saw_thumb = false;
    for (size_t step = 0; step < 512 && exits_ == previous_exits && !exception_;
         ++step) {
      // Require the actual MMU callback for the negative control, rather than
      // allowing a cached stack-page store to bypass it.
      if (inject_wrong_input) {
        cpu_->flush_tlb();
      }
      ObserveStep(cpu_->get_pc());
      cpu_->step();
      saw_thumb |= cpu_->is_thumb_mode();
    }
    ASSERT_FALSE(exception_) << "CPU exception at " << exception_address_;
    ASSERT_EQ(exits_, previous_exits + 1);
    EXPECT_TRUE(saw_thumb);
    EXPECT_EQ(injections_, inject_wrong_input ? 1 : 0);
    const int expected_reason = inject_wrong_input ? 42 : 0;
    RecordProperty("exit_reason", std::to_string(exit_reason_));
    RecordProperty("input_injections", std::to_string(injections_));
    RecordProperty("memory_model_released",
                   process->get_mem_model() ? "false" : "true");
    EXPECT_EQ(exit_reason_, expected_reason);
    EXPECT_EQ(exit_type_, eka2l1::kernel::entity_exit_type::kill);
    EXPECT_EQ(thread->get_exit_reason(), expected_reason);
    EXPECT_EQ(thread->get_exit_type(), eka2l1::kernel::entity_exit_type::kill);
    EXPECT_EQ(process->get_mem_model(), nullptr);  // Kernel released resources.
    cpu_->write_32bit = std::move(original_write);
  }

  fs::path original_directory_;
  fs::path directory_;
  eka2l1::config::state config_;
  eka2l1::arm::exclusive_monitor_instance monitor_;
  eka2l1::arm::core_instance cpu_;
  std::unique_ptr<eka2l1::memory_system> memory_;
  std::unique_ptr<eka2l1::ntimer> timer_;
  std::unique_ptr<eka2l1::io_system> io_;
  std::unique_ptr<eka2l1::config::app_settings> settings_;
  std::unique_ptr<eka2l1::kernel_system> kernel_;
  size_t exits_ = 0;
  size_t injections_ = 0;
  bool inject_wrong_input_ = false;
  uint32_t initial_sp_ = 0;
  int exit_reason_ = -1;
  eka2l1::kernel::entity_exit_type exit_type_ =
      eka2l1::kernel::entity_exit_type::pending;
  bool exception_ = false;
  uint32_t exception_address_ = 0;
};

}  // namespace symbian::testing

#endif  // SYMBIAN_TESTS_EKA2L1_PROCESS_ENVIRONMENT_H_
