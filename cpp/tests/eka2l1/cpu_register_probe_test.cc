// SPDX-License-Identifier: GPL-3.0-or-later
// Executes actual ARM thread-register reads on both macOS CPU backends.

#include <array>
#include <cstdint>
#include <memory>

#include <absl/base/nullability.h>
#include <cpu/arm_factory.h>
#include <gtest/gtest.h>

namespace {

using eka2l1::arm::core;
constexpr uint32_t kCodeBase = 0x8000;
constexpr std::array<uint32_t, 2> kInstructions = {
    0xee1d0f50,  // mrc p15, 0, r0, c13, c0, 2: TPIDRURW
    0xee1d1f70,  // mrc p15, 0, r1, c13, c0, 3: TPIDRURO
};

class CpuRegisterProbeTest
    : public ::testing::TestWithParam<arm_emulator_type> {
 protected:
  void SetUp() override {
    ASSERT_EQ(eka2l1::arm::resolve_emulator_type(GetParam()), GetParam());
    monitor_ = eka2l1::arm::create_exclusive_monitor(GetParam(), 1);
    ASSERT_NE(monitor_, nullptr);
    cpu_ = eka2l1::arm::create_core(monitor_.get(), GetParam());
    ASSERT_NE(cpu_, nullptr);
    const auto read = [](uint32_t address, uint32_t* absl_nonnull value) {
      if (address < kCodeBase || (address - kCodeBase) % 4 != 0 ||
          address - kCodeBase >= 0x1000) {
        return false;
      }
      const auto index = (address - kCodeBase) / 4;
      // Both translators can read ahead; the fixture maps a whole code page.
      *value = index < kInstructions.size() ? kInstructions[index] : 0xe1a00000;
      return true;
    };
    cpu_->read_code = read;
    cpu_->read_32bit = read;
    cpu_->exception_handler = [this](auto, uint32_t) {
      exception_ = true;
      cpu_->stop();
      return false;
    };
    cpu_->system_call_handler = [this](uint32_t) {
      exception_ = true;
      cpu_->stop();
    };
  }

  core::thread_context Context(uint32_t writable, uint32_t read_only) {
    core::thread_context context{};
    context.cpsr = 0x10;  // Unprivileged ARM mode.
    context.set_pc(kCodeBase);
    context.uprw = writable;
    context.uro = read_only;
    return context;
  }

  void ReadRegisters(uint32_t writable, uint32_t read_only) {
    ASSERT_EQ(cpu_->get_pc(), kCodeBase);
    cpu_->step();
    ASSERT_FALSE(exception_);
    ASSERT_EQ(cpu_->get_pc(), kCodeBase + 4);
    cpu_->step();
    ASSERT_FALSE(exception_);
    ASSERT_EQ(cpu_->get_pc(), kCodeBase + 8);
    EXPECT_EQ(cpu_->get_reg(0), writable);
    EXPECT_EQ(cpu_->get_reg(1), read_only);
  }

  eka2l1::arm::exclusive_monitor_instance monitor_;
  eka2l1::arm::core_instance cpu_;
  bool exception_ = false;
};

TEST_P(CpuRegisterProbeTest, ReadsDistinctThreadRegistersAfterContextSwitch) {
  for (const auto& values : {std::array<uint32_t, 2>{0x12345678, 0x87654321},
                             {0xfedcba98, 0x13579bdf},
                             {0xabcdef01, 0}}) {
    cpu_->load_context(Context(values[0], values[1]));
    ASSERT_NO_FATAL_FAILURE(ReadRegisters(values[0], values[1]));
    core::thread_context saved{};
    cpu_->save_context(saved);
    EXPECT_EQ(saved.uprw, values[0]);
    EXPECT_EQ(saved.uro, values[1]);
  }
}

TEST_P(CpuRegisterProbeTest, RestoresSavedReadOnlyRegister) {
  cpu_->load_context(Context(0x12345678, 0x87654321));
  core::thread_context saved{};
  cpu_->save_context(saved);
  cpu_->load_context(Context(0xfedcba98, 0x13579bdf));
  ASSERT_NO_FATAL_FAILURE(ReadRegisters(0xfedcba98, 0x13579bdf));
  cpu_->load_context(saved);
  ASSERT_NO_FATAL_FAILURE(ReadRegisters(0x12345678, 0x87654321));
}

INSTANTIATE_TEST_SUITE_P(Backends, CpuRegisterProbeTest,
                         ::testing::Values(arm_emulator_type::dyncom,
                                           arm_emulator_type::dynarmic));

}  // namespace
