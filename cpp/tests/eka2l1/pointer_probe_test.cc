// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <vector>

#include <absl/base/nullability.h>
#include <common/buffer.h>
#include <kernel/process.h>
#include <loader/e32img.h>

#include "process_environment.h"

namespace {

class PointerProbeTest : public symbian::testing::ProcessEnvironment {
 protected:
  void Populate(const std::filesystem::path& artifact) override {
    ProcessEnvironment::Populate(artifact);
    std::ifstream file(artifact, std::ios::binary);
    std::string bytes{std::istreambuf_iterator<char>(file),
                      std::istreambuf_iterator<char>()};
    eka2l1::common::ro_buf_stream stream(
        reinterpret_cast<uint8_t*>(bytes.data()), bytes.size());
    const auto image = eka2l1::loader::parse_e32img(&stream, true);
    ASSERT_TRUE(image.has_value());
    ASSERT_EQ(image->header.entry_point, 0);
    ASSERT_EQ(image->header.data_size, 0);
    ASSERT_EQ(image->header.dll_ref_table_count, 0);
    ASSERT_EQ(image->code_reloc_section.num_relocs, 8);
    descriptor_ = image->header_extended.exception_des & ~1U;
    ASSERT_NE(descriptor_, 0);
    linked_base_ = image->header.code_base;
    code_size_ = image->header.code_size;
    for (const auto& page : image->code_reloc_section.entries) {
      for (const uint16_t word : page.rels_info) {
        if (word == 0) {
          continue;
        }
        ASSERT_EQ(word & 0xf000, 0x1000);
        const uint32_t offset = page.base + (word & 0xfff);
        ASSERT_LE(offset + 4, code_size_);
        uint32_t target = 0;
        std::memcpy(&target,
                    image->data.data() + image->header.code_offset + offset, 4);
        pointers_.push_back({offset, target});
      }
    }
    ASSERT_EQ(pointers_.size(), 8);
  }

  void BeforeExecute(eka2l1::kernel::process* absl_nonnull process) override {
    const uint32_t base = process->get_entry_point_address();
    ASSERT_NE(base, linked_base_);
    functions_.clear();
    seen_.clear();
    size_t labels = 0, arm = 0, thumb = 0;
    for (const auto& pointer : pointers_) {
      const auto* absl_nonnull mapped = static_cast<const uint32_t*>(
          process->get_ptr_on_addr_space(base + pointer.offset));
      ASSERT_NE(mapped, nullptr);
      EXPECT_EQ(*mapped, pointer.target + (base - linked_base_));
      EXPECT_EQ(*mapped & 1, pointer.target & 1);
      // EHABI descriptors carry four address fields, including an end pointer.
      // Check their relocation above, but they are not dispatch destinations.
      if (pointer.offset >= descriptor_ && pointer.offset < descriptor_ + 16) {
        continue;
      }
      const uint32_t normalized = pointer.target & ~1U;
      ASSERT_GE(normalized, linked_base_);
      ASSERT_LT(normalized - linked_base_, code_size_);
      const auto* absl_nonnull target =
          static_cast<const char*>(process->get_ptr_on_addr_space(*mapped));
      ASSERT_NE(target, nullptr);
      if (pointer.target - linked_base_ + 7 <= code_size_ &&
          std::memcmp(target, "ymbian", 7) == 0) {
        ++labels;  // Includes a nonzero addend into read-only data.
      } else {
        functions_.push_back(*mapped);
        (*mapped & 1) ? ++thumb : ++arm;
      }
    }
    EXPECT_EQ(labels, 1);
    EXPECT_EQ(arm, 1);
    EXPECT_EQ(thumb, 2);
    RecordProperty("mapped_pointer_words", "4");
    RecordProperty("function_pointer_states", "one ARM, two Thumb");
    RecordProperty("data_pointer_addend", "1");
  }

  void ObserveStep(uint32_t pc) override {
    for (const uint32_t function : functions_) {
      if (pc == (function & ~1U)) {
        EXPECT_EQ(cpu_->is_thumb_mode(), (function & 1) != 0);
        seen_.insert(function);
      }
    }
  }

  void ExpectDispatch() {
    EXPECT_EQ(functions_.size(), 3);
    EXPECT_EQ(seen_.size(), 3);  // Both callbacks and the C++ virtual method.
    RecordProperty("indirect_targets_executed", std::to_string(seen_.size()));
  }

  struct Pointer {
    uint32_t offset;
    uint32_t target;
  };

  uint32_t descriptor_ = 0;
  uint32_t linked_base_ = 0;
  uint32_t code_size_ = 0;
  std::vector<Pointer> pointers_;
  std::vector<uint32_t> functions_;
  std::set<uint32_t> seen_;
};

TEST_P(PointerProbeTest, RelocatesTablesAndExecutesIndirectCalls) {
  ASSERT_NO_FATAL_FAILURE(Execute(false));
  ExpectDispatch();
}

TEST_P(PointerProbeTest, ChangedInputStillExecutesAllDispatchPaths) {
  ASSERT_NO_FATAL_FAILURE(Execute(true));
  ExpectDispatch();
}

TEST_P(PointerProbeTest, ReloadsPointerTablesAfterFailureExit) {
  ASSERT_NO_FATAL_FAILURE(Execute(true));
  ExpectDispatch();
  ASSERT_NO_FATAL_FAILURE(Execute(false));
  ExpectDispatch();
  EXPECT_EQ(exits_, 2);
}

INSTANTIATE_TEST_SUITE_P(Backends, PointerProbeTest,
                         ::testing::Values(arm_emulator_type::dyncom,
                                           arm_emulator_type::dynarmic));

}  // namespace
