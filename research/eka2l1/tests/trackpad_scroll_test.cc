// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include <QApplication>
#include <QFocusEvent>
#include <QTest>
#include <QWheelEvent>
#include <vector>

#include <cmath>

#include "gtest/gtest.h"
#include "qt/displaywidget.h"

namespace {
struct Touch {
  eka2l1::vec3 position;
  int action;
};

TEST(TrackpadScroll, PixelMovementMomentumAndEndAreOneTouchGesture) {
  display_widget widget;
  widget.resize(360, 640);
  std::vector<Touch> touches;
  widget.raw_mouse_event = [&](void*, eka2l1::vec3 position, int button,
                               int action, int pointer) {
    EXPECT_EQ(button, 0);
    EXPECT_EQ(pointer, 0);
    touches.push_back({.position = position, .action = action});
  };
  const auto send = [&](QPoint pixels, QPoint angles, Qt::ScrollPhase phase) {
    QWheelEvent wheel(QPointF(100, 200), QPointF(100, 200), pixels, angles,
                      Qt::NoButton, Qt::NoModifier, phase, false);
    QApplication::sendEvent(&widget, &wheel);
    EXPECT_TRUE(wheel.isAccepted());
  };
  send({}, {}, Qt::ScrollBegin);
  EXPECT_TRUE(touches.empty());  // A stationary gesture must not click.
  send({0, -3}, {}, Qt::ScrollUpdate);
  send({0, -2}, {}, Qt::ScrollMomentum);
  send({}, {}, Qt::ScrollEnd);
  ASSERT_EQ(touches.size(), 4);
  EXPECT_EQ(touches[0].action, 0);
  EXPECT_EQ(touches[1].action, 1);
  EXPECT_EQ(touches[2].action, 1);
  EXPECT_EQ(touches[3].action, 2);
  EXPECT_EQ(touches[2].position.y - touches[0].position.y,
            std::lround(-5 * widget.devicePixelRatioF()));
  EXPECT_EQ(touches[3].position.y, touches[2].position.y);
}

TEST(TrackpadScroll, AngleFractionsArePreservedAndIdleReleases) {
  display_widget widget;
  widget.resize(360, 640);
  std::vector<Touch> touches;
  widget.raw_mouse_event = [&](void*, eka2l1::vec3 position, int, int action,
                               int) {
    touches.push_back({.position = position, .action = action});
  };
  for (int i = 0; i < 3; ++i) {
    QWheelEvent wheel({}, {}, {}, {0, -1}, Qt::NoButton, Qt::NoModifier,
                      Qt::NoScrollPhase, false);
    QApplication::sendEvent(&widget, &wheel);
  }
  ASSERT_EQ(touches.size(), 4);
  EXPECT_EQ(touches.back().position.y - touches.front().position.y,
            std::lround(-widget.devicePixelRatioF()));
  QTest::qWait(200);
  ASSERT_EQ(touches.size(), 5);
  EXPECT_EQ(touches.back().action, 2);
}

TEST(TrackpadScroll, LongDragsRemainInBoundsAndFocusLossReleases) {
  display_widget widget;
  widget.resize(360, 640);
  std::vector<Touch> touches;
  widget.raw_mouse_event = [&](void*, eka2l1::vec3 position, int, int action,
                               int) {
    touches.push_back({.position = position, .action = action});
  };
  QWheelEvent wheel({}, {}, {0, -1700}, {}, Qt::NoButton, Qt::NoModifier,
                    Qt::ScrollUpdate, false);
  QApplication::sendEvent(&widget, &wheel);
  int distance = 0;
  for (std::size_t i = 1; i < touches.size(); ++i) {
    EXPECT_GE(touches[i].position.y, 0);
    EXPECT_LT(touches[i].position.y,
              widget.height() * widget.devicePixelRatioF());
    if (touches[i].action == 1) {
      distance += touches[i].position.y - touches[i - 1].position.y;
    }
  }
  EXPECT_NEAR(distance, -1700 * widget.devicePixelRatioF(), 2);
  QFocusEvent lost(QEvent::FocusOut);
  QApplication::sendEvent(&widget, &lost);
  ASSERT_FALSE(touches.empty());
  EXPECT_EQ(touches.back().action, 2);
}
}  // namespace

int main(int argc, char** argv) {
  QApplication application(argc, argv);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
