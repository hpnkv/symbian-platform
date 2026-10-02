// SPDX-License-Identifier: GPL-3.0-or-later
// A ROM-independent regression for the pinned Dyncom STREXD value path.

#include <cstdint>
#include <cstring>
#include <vector>

#include <cpu/arm_factory.h>
#include <gtest/gtest.h>

namespace {

constexpr uint32_t kCode = 0x8000;
constexpr uint32_t kData = 0x10000;
constexpr uint32_t kMemorySize = 0x20000;
// ARM: LDREXD r6, r7, [r8]; STREXD r6, r2, r3, [r8].
constexpr uint32_t kLdrexd = 0xE1B86F9F;
constexpr uint32_t kStrexd = 0xE1A86F92;

class StrexdTest : public ::testing::TestWithParam<arm_emulator_type> {
 protected:
  void SetUp() override {
    memory_.resize(kMemorySize);
    std::memcpy(memory_.data() + kCode, &kLdrexd, sizeof(kLdrexd));
    std::memcpy(memory_.data() + kCode + 4, &kStrexd, sizeof(kStrexd));
    monitor_ = eka2l1::arm::create_exclusive_monitor(GetParam(), 1);
    ASSERT_NE(monitor_, nullptr);
    monitor_->read_64bit = [this](eka2l1::arm::core*, uint32_t address,
                                  uint64_t* value) {
      return Read(address, value);
    };
    monitor_->write_64bit = [this](eka2l1::arm::core*, uint32_t address,
                                   uint64_t value, uint64_t expected) {
      uint64_t current = 0;
      if (!Read(address, &current) || current != expected) {
        return 0;
      }
      return Write(address, &value) ? 1 : 0;
    };
    cpu_ = eka2l1::arm::create_core(monitor_.get(), GetParam());
    ASSERT_NE(cpu_, nullptr);
    cpu_->read_code = [this](uint32_t address, uint32_t* value) {
      return Read(address, value);
    };
    cpu_->read_8bit = [this](uint32_t address, uint8_t* value) {
      return Read(address, value);
    };
    cpu_->read_16bit = [this](uint32_t address, uint16_t* value) {
      return Read(address, value);
    };
    cpu_->read_32bit = [this](uint32_t address, uint32_t* value) {
      return Read(address, value);
    };
    cpu_->read_64bit = [this](uint32_t address, uint64_t* value) {
      return Read(address, value);
    };
    cpu_->write_8bit = [this](uint32_t address, uint8_t* value) {
      return Write(address, value);
    };
    cpu_->write_16bit = [this](uint32_t address, uint16_t* value) {
      return Write(address, value);
    };
    cpu_->write_32bit = [this](uint32_t address, uint32_t* value) {
      return Write(address, value);
    };
    cpu_->write_64bit = [this](uint32_t address, uint64_t* value) {
      return Write(address, value);
    };
    cpu_->exclusive_write_64bit = [this](uint32_t address, uint64_t value,
                                         uint64_t expected) {
      uint64_t current = 0;
      if (!Read(address, &current) || current != expected) {
        return 0;
      }
      return Write(address, &value) ? 1 : 0;
    };
    eka2l1::arm::core::thread_context context{};
    context.cpsr = 0x10;
    context.set_pc(kCode);
    cpu_->load_context(context);
    cpu_->set_reg(8, kData);
  }

  template <typename T>
  bool Read(uint32_t address, T* value) {
    if (address > memory_.size() || sizeof(T) > memory_.size() - address) {
      return false;
    }
    std::memcpy(value, memory_.data() + address, sizeof(T));
    return true;
  }

  template <typename T>
  bool Write(uint32_t address, T* value) {
    if (address < kData || address > memory_.size() ||
        sizeof(T) > memory_.size() - address) {
      return false;
    }
    std::memcpy(memory_.data() + address, value, sizeof(T));
    return true;
  }

  std::vector<uint8_t> memory_;
  eka2l1::arm::exclusive_monitor_instance monitor_;
  eka2l1::arm::core_instance cpu_;
};

TEST_P(StrexdTest, WritesBothRegistersAndReportsSuccess) {
  constexpr uint64_t initial = 0x1122334455667788ULL;
  std::memcpy(memory_.data() + kData, &initial, sizeof(initial));
  cpu_->set_reg(2, 0xAABBCCDD);
  cpu_->set_reg(3, 0x12345678);
  cpu_->step();
  EXPECT_EQ(cpu_->get_reg(6), 0x55667788U);
  EXPECT_EQ(cpu_->get_reg(7), 0x11223344U);
  cpu_->step();
  EXPECT_EQ(cpu_->get_reg(6), 0U);
  uint64_t stored = 0;
  std::memcpy(&stored, memory_.data() + kData, sizeof(stored));
  EXPECT_EQ(stored, 0x12345678AABBCCDDULL);
}

TEST_P(StrexdTest, ChangedHighRegisterChangesStoredValue) {
  cpu_->set_reg(2, 0xAABBCCDD);
  cpu_->set_reg(3, 0x87654321);
  cpu_->step();
  cpu_->step();
  uint64_t stored = 0;
  std::memcpy(&stored, memory_.data() + kData, sizeof(stored));
  EXPECT_EQ(stored, 0x87654321AABBCCDDULL);
}

INSTANTIATE_TEST_SUITE_P(Backends, StrexdTest,
                         ::testing::Values(arm_emulator_type::dyncom,
                                           arm_emulator_type::dynarmic));

}  // namespace
