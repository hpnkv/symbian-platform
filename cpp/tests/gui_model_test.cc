#include <array>
#include <cstdint>
#include <limits>

#include <gtest/gtest.h>

#include "model.h"

namespace {

using gui_app::Divide;
using gui_app::MakeLayout;
using gui_app::Model;

TEST(GuiModelTest, DivisionMatchesIntegerArithmeticAtEdgesAndMixedValues) {
  const std::array<uint32_t, 8> divisors = {1,  3,     6,          10,
                                            28, 65537, 0x80000000, UINT32_MAX};
  const std::array<uint32_t, 8> edges = {
      0, 1, 9, 10, 9999, 0x7fffffff, 0x80000000, UINT32_MAX};
  for (const uint32_t divisor : divisors) {
    for (const uint32_t value : edges) {
      const auto result = Divide(value, divisor);
      EXPECT_EQ(result.quotient, value / divisor);
      EXPECT_EQ(result.remainder, value % divisor);
    }
    uint32_t value = 0x808U;
    for (int i = 0; i < 1000; ++i) {
      value = value * 1664525U + 1013904223U;
      const auto result = Divide(value, divisor);
      EXPECT_EQ(result.quotient, value / divisor);
      EXPECT_EQ(result.remainder, value % divisor);
    }
  }
}

TEST(GuiModelTest, ControlsStayInsideSupportedPortraitAndLandscapeScreens) {
  for (const auto size :
       {std::array{120, 160}, std::array{360, 640}, std::array{640, 360},
        std::array{8192, 8192}, std::array{8192, 160}, std::array{120, 8192}}) {
    const auto layout = MakeLayout(size[0], size[1]);
    const int scale = gui_app::DigitScale(layout);
    ASSERT_GT(scale, 0);
    EXPECT_LE(23 * scale, layout.width);
    EXPECT_LE(gui_app::DividePositive(layout.height, 4) + 9 * scale,
              layout.increment.y - 12);
    EXPECT_LT(layout.increment.x + layout.increment.width, layout.reset.x);
    EXPECT_LT(layout.reset.x + layout.reset.width, layout.exit.x);
    EXPECT_LT(layout.exit.x + layout.exit.width, layout.width);
    for (const auto button : {layout.increment, layout.reset, layout.exit}) {
      EXPECT_GT(button.width, 0);
      EXPECT_GT(button.height, 0);
      EXPECT_LT(button.y + button.height, layout.height);
    }
  }
}

TEST(GuiModelTest, PointerHitRegionsAreHalfOpenAndGapsDoNothing) {
  const auto layout = MakeLayout(360, 640);
  Model model;
  EXPECT_FALSE(model.Tap(layout, layout.increment.x - 1, layout.increment.y));
  EXPECT_FALSE(model.Tap(layout, layout.increment.x + layout.increment.width,
                         layout.increment.y));
  EXPECT_FALSE(model.Tap(layout, layout.increment.x,
                         layout.increment.y + layout.increment.height));
  EXPECT_FALSE(model.Tap(layout, std::numeric_limits<int>::min(), 0));
  EXPECT_EQ(model.count(), 0);
  EXPECT_TRUE(model.Tap(layout, layout.increment.x, layout.increment.y));
  EXPECT_EQ(model.count(), 1);
}

TEST(GuiModelTest, CounterSaturatesWithoutOverflowAndResetClearsIt) {
  const auto layout = MakeLayout(360, 640);
  Model model;
  for (int i = 0; i < 10001; ++i) {
    ASSERT_TRUE(model.Tap(layout, layout.increment.x, layout.increment.y));
  }
  EXPECT_EQ(model.count(), 9999);
  EXPECT_TRUE(model.Tap(layout, layout.reset.x, layout.reset.y));
  EXPECT_EQ(model.count(), 0);
  EXPECT_TRUE(model.running());
}

TEST(GuiModelTest, ExitStopsFurtherInput) {
  const auto layout = MakeLayout(360, 640);
  Model model;
  EXPECT_TRUE(model.Tap(layout, layout.exit.x, layout.exit.y));
  EXPECT_FALSE(model.running());
  EXPECT_FALSE(model.Tap(layout, layout.increment.x, layout.increment.y));
  EXPECT_FALSE(model.Tap(layout, layout.reset.x, layout.reset.y));
  EXPECT_EQ(model.count(), 0);
}

}  // namespace
