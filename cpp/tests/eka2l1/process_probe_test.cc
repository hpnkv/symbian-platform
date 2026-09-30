// SPDX-License-Identifier: GPL-3.0-or-later
#include "process_environment.h"

namespace {
using ProcessProbeTest = symbian::testing::ProcessEnvironment;

TEST_P(ProcessProbeTest, LoadsRunsAndExitsThroughKernel) {
  ASSERT_NO_FATAL_FAILURE(Execute(false));
}

TEST_P(ProcessProbeTest, ChangedInputProducesKernelFailureExit) {
  ASSERT_NO_FATAL_FAILURE(Execute(true));
}

TEST_P(ProcessProbeTest, CanLaunchAgainAfterKernelExit) {
  ASSERT_NO_FATAL_FAILURE(Execute(true));
  ASSERT_NO_FATAL_FAILURE(Execute(false));
  EXPECT_EQ(exits_, 2);
  RecordProperty("completed_processes", std::to_string(exits_));
}

TEST_P(ProcessProbeTest, MissingExecutableCannotCreateProcess) {
  EXPECT_EQ(kernel_->spawn_new_process(u"C:\\sys\\bin\\missing.exe", u""),
            nullptr);
  EXPECT_EQ(exits_, 0);
}

INSTANTIATE_TEST_SUITE_P(Backends, ProcessProbeTest,
                         ::testing::Values(arm_emulator_type::dyncom,
                                           arm_emulator_type::dynarmic));

}  // namespace
