// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef EXAMPLES_CAMERA_APP_PREVIEW_CONTROLS_H_
#define EXAMPLES_CAMERA_APP_PREVIEW_CONTROLS_H_

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "symbian/api/display/window_surface.h"

namespace camera_app {

// Map a logical rectangle onto the native EGL drawable. Controls keep their
// physical anchor while their labels follow the current logical orientation.
inline symbian::api::display::WindowRect NativePreviewRect(
    symbian::api::display::WindowRect rect,
    symbian::api::display::WindowSize logical_size,
    symbian::api::display::DisplayRotation rotation) {
  using symbian::api::display::DisplayRotation;
  switch (rotation) {
    case DisplayRotation::k90:
      return {logical_size.height - rect.y - rect.height, rect.x, rect.height,
              rect.width};
    case DisplayRotation::k180:
      return {logical_size.width - rect.x - rect.width,
              logical_size.height - rect.y - rect.height, rect.width,
              rect.height};
    case DisplayRotation::k270:
      return {rect.y, logical_size.width - rect.x - rect.width, rect.height,
              rect.width};
    case DisplayRotation::k0:
      return rect;
  }
  return rect;
}

inline symbian::api::display::WindowRect PreviewButton(
    symbian::api::display::WindowSize size,
    symbian::api::display::DisplayRotation rotation) {
  const int turn = static_cast<int>(rotation);
  const int native_width = turn % 2 ? size.height : size.width;
  const int native_height = turn % 2 ? size.width : size.height;
  const int scale = std::min(native_width, native_height) >= 320 ? 2 : 1;
  const int physical_x = native_width / 2;
  const int physical_y = native_height - 28 * scale;
  int x = physical_x;
  int y = physical_y;
  switch (rotation) {
    case symbian::api::display::DisplayRotation::k0:
      break;
    case symbian::api::display::DisplayRotation::k90:
      x = physical_y;
      y = native_width - physical_x;
      break;
    case symbian::api::display::DisplayRotation::k180:
      x = native_width - physical_x;
      y = native_height - physical_y;
      break;
    case symbian::api::display::DisplayRotation::k270:
      x = native_height - physical_y;
      y = physical_x;
      break;
  }
  return {x - 22 * scale, y - 9 * scale, 44 * scale, 18 * scale};
}

// Tiny glyphs for the smoothing control. GLES has no native text compositor;
// rendering with the same rectangle sink keeps labels on the camera surface.
inline std::array<unsigned char, 7> ModeGlyph(char letter) {
  switch (letter) {
    case 'S':
      return {15, 16, 16, 14, 1, 1, 30};
    case 'M':
      return {17, 27, 21, 21, 17, 17, 17};
    case 'H':
      return {17, 17, 17, 31, 17, 17, 17};
    case 'O':
      return {14, 17, 17, 17, 17, 17, 14};
    case 'T':
      return {31, 4, 4, 4, 4, 4, 4};
    default:
      return {};
  }
}

// Emit uses top-left pixel coordinates and 0xRRGGBB colors. Both software and
// GLES renderers share the layout, rounding, labels and hit rectangles.
template <typename Emit>
void DrawPreviewButton(symbian::api::display::WindowSize size,
                       symbian::api::display::DisplayRotation rotation,
                       bool smooth, Emit emit) {
  const auto rect = PreviewButton(size, rotation);
  const int scale = rect.height / 18;
  const std::uint32_t border = smooth ? 0x43c4cc : 0x53636e;
  const std::uint32_t background = smooth ? 0x18383e : 0x121b23;
  const int radius = 4 * scale;
  for (int row = 0; row < rect.height; ++row) {
    const int dy = std::max(radius - row - 1, row - (rect.height - radius));
    int dx = radius;
    while (dy > 0 && dx * dx + dy * dy > radius * radius) {
      --dx;
    }
    const int inset = dy > 0 ? radius - dx : 0;
    emit(rect.x + inset, rect.y + row, rect.width - 2 * inset, 1, border);
    if (row > 0 && row < rect.height - 1) {
      emit(rect.x + inset + 1, rect.y + row, rect.width - 2 * inset - 2, 1,
           background);
    }
  }
  const std::string_view label = "SMOOTH";
  int x = rect.x +
          (rect.width - (static_cast<int>(label.size()) * 6 - 1) * scale) / 2;
  const int y = rect.y + (rect.height - 7 * scale) / 2;
  for (char letter : label) {
    const auto glyph = ModeGlyph(letter);
    for (int row = 0; row < 7; ++row) {
      for (int column = 0; column < 5; ++column) {
        if (glyph[static_cast<std::size_t>(row)] & (16 >> column)) {
          emit(x + column * scale, y + row * scale, scale, scale,
               smooth ? 0xe6ffff : 0xc0cbd3);
        }
      }
    }
    x += 6 * scale;
  }
}

}  // namespace camera_app

#endif  // EXAMPLES_CAMERA_APP_PREVIEW_CONTROLS_H_
