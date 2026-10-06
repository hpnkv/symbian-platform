// SPDX-License-Identifier: GPL-3.0-or-later
// Executive routing controls. Actual ROM execution is a separate live test.

#include <string>

#include <absl/base/nullability.h>
#include <gtest/gtest.h>
#include <kernel/svc.h>

namespace {

const char* absl_nonnull HandlerName(const eka2l1::hle::func_map& table,
                                     unsigned number) {
  const auto found = table.find(number);
  return found == table.end() ? nullptr : found->second.name.c_str();
}

TEST(SvcProfileProbeTest, KeepsOriginalSymbian3RoutingAvailable) {
  const auto& original = eka2l1::epoc::svc_register_funcs_v10;
  RecordProperty("registered_handlers", std::to_string(original.size()));
  EXPECT_STREQ(HandlerName(original, 0x4f), "hal_function");
  EXPECT_STREQ(HandlerName(original, 0x6b), "chunk_new");
  EXPECT_STREQ(HandlerName(original, 0x6d), "handle_open_object");
  EXPECT_STREQ(HandlerName(original, 0x73), "thread_kill");
  EXPECT_EQ(original.count(0x51), 0);
  EXPECT_EQ(original.count(0xf7), 0);
}

TEST(SvcProfileProbeTest, RoutesObservedRomCallsToTheirExistingHandlers) {
  const auto& profile = eka2l1::epoc::svc_register_funcs_v101;
  RecordProperty("registered_handlers", std::to_string(profile.size()));
  // Independent source/export wrapper observations span both shift boundaries.
  EXPECT_STREQ(HandlerName(profile, 0x0e), "library_lookup");
  EXPECT_STREQ(HandlerName(profile, 0x13), "mutex_wait_ver2");
  EXPECT_STREQ(HandlerName(profile, 0x51), "hal_function");
  EXPECT_STREQ(HandlerName(profile, 0x6c), "handle_close");
  EXPECT_STREQ(HandlerName(profile, 0x6d), "chunk_new");
  EXPECT_STREQ(HandlerName(profile, 0x6e), "chunk_adjust");
  EXPECT_STREQ(HandlerName(profile, 0x75), "thread_kill");
  EXPECT_STREQ(HandlerName(profile, 0xbf), "property_define");
  EXPECT_STREQ(HandlerName(profile, 0xd9), "condvar_create");
  EXPECT_STREQ(HandlerName(profile, 0xe1), "leave_start");
  EXPECT_STREQ(HandlerName(profile, 0xf7), "thread_user_exiting");
  EXPECT_STREQ(HandlerName(profile, 0x10c), "static_call_done");
  EXPECT_STREQ(HandlerName(profile, 0x10d), "library_entry_call_start");
  EXPECT_STREQ(HandlerName(profile, 0x10e), "library_load_prepare");
  EXPECT_STREQ(HandlerName(profile, 0x800002), "heap_switch");
}

TEST(SvcProfileProbeTest, LeavesUnverifiedSlotsUnregistered) {
  const auto& profile = eka2l1::epoc::svc_register_funcs_v101;
  EXPECT_EQ(profile.count(0x11), 0);
  EXPECT_EQ(profile.count(0x12), 0);
  EXPECT_EQ(profile.count(0xff), 0);
}

}  // namespace
