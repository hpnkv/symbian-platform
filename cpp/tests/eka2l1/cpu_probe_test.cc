// SPDX-License-Identifier: GPL-3.0-or-later
// Executes the maintained E32 code in EKA2L1's CPU cores, without a ROM/kernel.
// The exit SVC is observed by a test callback, not dispatched to Symbian.

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include <absl/base/nullability.h>
#include <common/buffer.h>
#include <cpu/arm_factory.h>
#include <gtest/gtest.h>
#include <loader/e32img.h>

namespace {

using eka2l1::arm::core;
constexpr uint32_t kMemorySize = 0x80000;
constexpr uint32_t kStackBase = 0x60000;
constexpr uint32_t kStackTop = 0x70000;
constexpr uint32_t kCodeMappingSize = 0x1000;

// A synchronous fixture with read-only code and a private writable stack.
// No emulator scheduler, OS services, Python callbacks or target SDK are used.
class CpuProbeTest
    : public ::testing::TestWithParam<std::tuple<arm_emulator_type, uint32_t>> {
 protected:
  void SetUp() override {
    const char* absl_nullable path = std::getenv("SYMBIAN_E32_TEST_IMAGE");
    ASSERT_NE(path, nullptr);
    std::ifstream file(path, std::ios::binary);
    ASSERT_TRUE(file.is_open()) << path;
    std::string bytes{std::istreambuf_iterator<char>(file),
                      std::istreambuf_iterator<char>()};
    ASSERT_GE(bytes.size(), 156);
    eka2l1::common::ro_buf_stream stream(
        reinterpret_cast<uint8_t*>(bytes.data()), bytes.size());
    const auto image = eka2l1::loader::parse_e32img(&stream, true);
    ASSERT_TRUE(image.has_value());
    ASSERT_EQ(image->header.data_size, 0);
    ASSERT_EQ(image->header.dll_ref_table_count, 0);
    ASSERT_EQ(image->code_reloc_section.num_relocs, 4);
    ASSERT_LE(image->header.code_size, kCodeMappingSize);
    ASSERT_LT(image->header.entry_point, image->header.code_size);
    base_ = std::get<1>(GetParam());
    memory_.resize(kMemorySize);
    std::memcpy(memory_.data() + base_,
                image->data.data() + image->header.code_offset,
                image->header.code_size);
    // This isolated CPU harness supplies the loader's text relocations before
    // executing at each independent base; the process oracle uses its loader.
    for (const auto& page : image->code_reloc_section.entries) {
      for (const uint16_t word : page.rels_info) {
        if (word == 0) {
          continue;
        }
        ASSERT_EQ(word & 0xf000, 0x1000);
        const uint32_t offset = page.base + (word & 0xfff);
        ASSERT_LE(offset + 4, image->header.code_size);
        uint32_t value = 0;
        std::memcpy(&value, memory_.data() + base_ + offset, 4);
        value += base_ - image->header.code_base;
        std::memcpy(memory_.data() + base_ + offset, &value, 4);
      }
    }
    const auto backend = std::get<0>(GetParam());
    ASSERT_EQ(eka2l1::arm::resolve_emulator_type(backend), backend);
    monitor_ = eka2l1::arm::create_exclusive_monitor(backend, 1);
    ASSERT_NE(monitor_, nullptr);
    cpu_ = eka2l1::arm::create_core(monitor_.get(), backend);
    ASSERT_NE(cpu_, nullptr);
    BindMemory();
    cpu_->exception_handler = [this](auto, uint32_t address) {
      exception_ = true;
      exception_address_ = address;
      cpu_->stop();
      return false;
    };
    cpu_->system_call_handler = [this](uint32_t svc) {
      svc_ = svc;
      svc_seen_ = true;
      for (size_t i = 0; i < arguments_.size(); ++i) {
        arguments_[i] = cpu_->get_reg(i);
      }
      cpu_->stop();
    };
    eka2l1::arm::core::thread_context context{};
    context.cpsr = 0x10;  // User mode, ARM entry.
    context.set_pc(base_ + image->header.entry_point);
    context.set_sp(kStackTop);
    cpu_->load_context(context);
  }

  bool IsCode(uint32_t address, size_t size) const {
    return address >= base_ && address - base_ <= kCodeMappingSize &&
           size <= kCodeMappingSize - (address - base_);
  }

  bool IsStack(uint32_t address, size_t size) const {
    return address >= kStackBase && address <= kStackTop &&
           size <= kStackTop - address;
  }

  template <typename T>
  bool Read(uint32_t address, T* absl_nonnull value) {
    if (!IsCode(address, sizeof(T)) && !IsStack(address, sizeof(T))) {
      return false;
    }
    std::memcpy(value, memory_.data() + address, sizeof(T));
    return true;
  }

  template <typename T>
  bool Write(uint32_t address, T* absl_nonnull value) {
    if (!IsStack(address, sizeof(T))) {
      return false;
    }
    std::memcpy(memory_.data() + address, value, sizeof(T));
    return true;
  }

  void BindMemory() {
    cpu_->read_code = [this](uint32_t address, uint32_t* absl_nonnull value) {
      return IsCode(address, sizeof(*value)) && Read(address, value);
    };
    cpu_->read_8bit = [this](uint32_t a, uint8_t* absl_nonnull v) {
      return Read(a, v);
    };
    cpu_->read_16bit = [this](uint32_t a, uint16_t* absl_nonnull v) {
      return Read(a, v);
    };
    cpu_->read_32bit = [this](uint32_t a, uint32_t* absl_nonnull v) {
      return Read(a, v);
    };
    cpu_->read_64bit = [this](uint32_t a, uint64_t* absl_nonnull v) {
      return Read(a, v);
    };
    cpu_->write_8bit = [this](uint32_t a, uint8_t* absl_nonnull v) {
      return Write(a, v);
    };
    cpu_->write_16bit = [this](uint32_t a, uint16_t* absl_nonnull v) {
      return Write(a, v);
    };
    cpu_->write_32bit = [this](uint32_t a, uint32_t* absl_nonnull v) {
      if (inject_wrong_input_ && *v == 16 && IsStack(a, sizeof(*v))) {
        const uint32_t changed = 17;
        ++input_injections_;
        std::memcpy(memory_.data() + a, &changed, sizeof(changed));
        return true;
      }
      return Write(a, v);
    };
    cpu_->write_64bit = [this](uint32_t a, uint64_t* absl_nonnull v) {
      return Write(a, v);
    };
  }

  void Execute() {
    for (size_t i = 0; i < 512 && !svc_seen_ && !exception_; ++i) {
      cpu_->step();
      saw_thumb_ |= cpu_->is_thumb_mode();
    }
    ASSERT_FALSE(exception_) << "Exception at " << exception_address_;
    ASSERT_TRUE(svc_seen_) << "Probe failed to reach exit within 512 steps";
    EXPECT_TRUE(saw_thumb_);
    EXPECT_FALSE(cpu_->is_thumb_mode());  // Returned to ARM startup.
    EXPECT_EQ(cpu_->get_sp(), kStackTop);
    EXPECT_EQ(svc_, 0x73);
    EXPECT_EQ(arguments_[0], 0xffff8001);
    EXPECT_EQ(arguments_[1], 0);
    EXPECT_EQ(arguments_[3], 0);
  }

  std::vector<uint8_t> memory_;
  eka2l1::arm::exclusive_monitor_instance monitor_;
  eka2l1::arm::core_instance cpu_;
  uint32_t base_ = 0;
  bool svc_seen_ = false;
  bool exception_ = false;
  bool saw_thumb_ = false;
  bool inject_wrong_input_ = false;
  uint32_t exception_address_ = 0;
  uint32_t svc_ = 0;
  size_t input_injections_ = 0;
  std::array<uint32_t, 4> arguments_{};
};

TEST_P(CpuProbeTest, ExecutesArmStartupThumbCppAndExitContract) {
  ASSERT_NO_FATAL_FAILURE(Execute());
  EXPECT_EQ(arguments_[2], 0);
}

TEST_P(CpuProbeTest, ChangedInputProducesFailureExit) {
  // A negative control changes the volatile stack input, not the expected
  // result. This checks that the calculation/comparison really executes.
  inject_wrong_input_ = true;
  ASSERT_NO_FATAL_FAILURE(Execute());
  EXPECT_EQ(input_injections_, 1);
  EXPECT_EQ(arguments_[2], 42);
}

INSTANTIATE_TEST_SUITE_P(
    BackendsAndAddresses, CpuProbeTest,
    ::testing::Combine(::testing::Values(arm_emulator_type::dyncom,
                                         arm_emulator_type::dynarmic),
                       ::testing::Values(uint32_t{0x8000}, uint32_t{0x20000})));

}  // namespace
