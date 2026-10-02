#include <string>

#include "app_bridge.h"
#include "gtest/gtest.h"

namespace {

TEST(AppStarter, BoundedLogKeepsLatestLinesAndClearReusesModel) {
  void* app = AppCreate();
  ASSERT_NE(app, nullptr);
  EXPECT_EQ(AppLineCount(app), 0);
  for (int day = 1; day <= 15; ++day) {
    AppLogTime(app, {2026, 10, day, 9, 7, 3});
  }
  ASSERT_EQ(AppLineCount(app), 12);
  EXPECT_EQ(std::string(AppLine(app, 0)), "2026-10-04 09:07:03");
  EXPECT_EQ(std::string(AppLine(app, 11)), "2026-10-15 09:07:03");
  AppClear(app);
  EXPECT_EQ(AppLineCount(app), 0);
  AppLogTime(app, {2027, 1, 1, 0, 0, 0});
  EXPECT_EQ(std::string(AppLine(app, 0)), "2027-01-01 00:00:00");
  AppDestroy(app);
}

}  // namespace
