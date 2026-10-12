// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include <QImage>
#include <QRect>
#include <algorithm>

#include <cmath>
#include <gtest/gtest.h>

#include "camera_image_qt.h"

namespace {

using eka2l1::drivers::camera::prepare_camera_image;

// A centered square reveals anisotropic scaling. Coordinate gradients expose
// crop bounds independently of the production geometry calculations.
QImage CalibrationImage() {
  QImage image(640, 480, QImage::Format_RGB32);
  for (int y = 0; y < image.height(); ++y) {
    for (int x = 0; x < image.width(); ++x) {
      image.setPixelColor(x, y, QColor(x * 255 / 639, y * 255 / 479, 0));
    }
  }
  for (int y = 192; y < 288; ++y) {
    for (int x = 272; x < 368; ++x) {
      image.setPixelColor(x, y, Qt::white);
    }
  }
  return image;
}

TEST(CameraImage, CenteredCropPreservesProportionsAndOneCompleteAxis) {
  const auto source = CalibrationImage();
  for (const auto size : {QSize(360, 640), QSize(640, 360)}) {
    for (int rotation : {0, 90, 180, 270}) {
      SCOPED_TRACE(::testing::Message() << size.width() << 'x' << size.height()
                                        << " rotation=" << rotation);
      const auto output =
          prepare_camera_image(source, size.width(), size.height(), rotation);
      ASSERT_EQ(output.size(), size);
      QRect square;
      for (int y = 0; y < output.height(); ++y) {
        for (int x = 0; x < output.width(); ++x) {
          if (output.pixelColor(x, y).blue() > 240) {
            square |= QRect(x, y, 1, 1);
          }
        }
      }
      ASSERT_FALSE(square.isEmpty());
      EXPECT_LE(std::abs(square.width() - square.height()), 1);
      EXPECT_LE(std::abs(square.center().x() - output.width() / 2), 1);
      EXPECT_LE(std::abs(square.center().y() - output.height() / 2), 1);

      // The limiting axis must reach both extremes of the coordinate gradient;
      // an extra zoom, a letterbox or a crop on both axes fails these checks.
      const bool quarter_turn = rotation == 90 || rotation == 270;
      const int width = quarter_turn ? source.height() : source.width();
      const int height = quarter_turn ? source.width() : source.height();
      const bool full_height = width * size.height() > height * size.width();
      const auto first = output.pixelColor(0, 0);
      const auto last =
          output.pixelColor(output.width() - 1, output.height() - 1);
      const bool green_axis = full_height != quarter_turn;
      const int a = green_axis ? first.green() : first.red();
      const int b = green_axis ? last.green() : last.red();
      EXPECT_LE(std::min(a, b), 1);
      EXPECT_GE(std::max(a, b), 254);
      const double scale = full_height
                               ? static_cast<double>(size.height()) / height
                               : static_cast<double>(size.width()) / width;
      EXPECT_NEAR(square.width(), 96 * scale, 2);
      EXPECT_NEAR(square.height(), 96 * scale, 2);
    }
  }
}

TEST(CameraImage, InvalidDimensionsDoNotProduceAnImage) {
  EXPECT_TRUE(prepare_camera_image({}, 360, 640, 0).isNull());
  EXPECT_TRUE(prepare_camera_image(CalibrationImage(), 0, 640, 0).isNull());
  EXPECT_TRUE(prepare_camera_image(CalibrationImage(), 360, -1, 0).isNull());
}

}  // namespace
