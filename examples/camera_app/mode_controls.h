// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef EXAMPLES_CAMERA_APP_MODE_CONTROLS_H_
#define EXAMPLES_CAMERA_APP_MODE_CONTROLS_H_

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

#include "viewfinder_presentation.h"

namespace camera_app {

inline auto ModeButtons(symbian::api::display::WindowSize size,
                        symbian::api::display::DisplayRotation rotation) {
  using symbian::api::display::WindowRect;
  const int turn = static_cast<int>(rotation);
  const int native_width = turn % 2 ? size.height : size.width;
  const int native_height = turn % 2 ? size.width : size.height;
  const int scale = std::min(native_width, native_height) >= 320 ? 2 : 1;
  std::array<WindowRect, 2> buttons;
  for (std::size_t index = 0; index < buttons.size(); ++index) {
    // Keep the group's center on the physical glass. Recenter each pill around
    // it with a four-unit gap, using the pill's rotated extent along this axis.
    // Reserve enough edge space for the long axis in either orientation.
    const int half_spacing = (turn % 2 ? 11 : 24) * scale;
    const int physical_x =
        native_width / 2 + (index == 0 ? -half_spacing : half_spacing);
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
    buttons[index] = {x - 22 * scale, y - 9 * scale, 44 * scale, 18 * scale};
  }
  return buttons;
}

inline std::optional<ViewfinderMode> HitModeButton(
    symbian::api::display::WindowSize size,
    symbian::api::display::DisplayRotation rotation, int x, int y) {
  const auto buttons = ModeButtons(size, rotation);
  for (std::size_t index = 0; index < buttons.size(); ++index) {
    const auto rect = buttons[index];
    // Keep touch areas disjoint, including on small displays.
    const int padding = rect.height >= 36 ? 4 : 2;
    if (x >= rect.x - padding && x < rect.x + rect.width + padding &&
        y >= rect.y - padding && y < rect.y + rect.height + padding) {
      return index == 0 ? ViewfinderMode::kRearCamera
                        : ViewfinderMode::kRotateView;
    }
  }
  return std::nullopt;
}

// Five tiny glyphs for the two labels. GLES has no native text compositor;
// rendering with the same rectangle sink keeps labels on the camera surface.
inline std::array<unsigned char, 7> ModeGlyph(char letter) {
  switch (letter) {
    case 'R':
      return {30, 17, 17, 30, 20, 18, 17};
    case 'E':
      return {31, 16, 16, 30, 16, 16, 31};
    case 'A':
      return {14, 17, 17, 31, 17, 17, 17};
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
void DrawModeButtons(symbian::api::display::WindowSize size,
                     symbian::api::display::DisplayRotation rotation,
                     ViewfinderMode selected, Emit emit) {
  const auto buttons = ModeButtons(size, rotation);
  const int scale = buttons[0].height / 18;
  for (std::size_t index = 0; index < buttons.size(); ++index) {
    const auto rect = buttons[index];
    const bool active =
        index == (selected == ViewfinderMode::kRearCamera ? 0 : 1);
    const std::uint32_t border = active ? 0x43c4cc : 0x53636e;
    const std::uint32_t background = active ? 0x18383e : 0x121b23;
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
    const std::string_view label = index == 0 ? "REAR" : "ROTATE";
    int x = rect.x +
            (rect.width - (static_cast<int>(label.size()) * 6 - 1) * scale) / 2;
    const int y = rect.y + (rect.height - 7 * scale) / 2;
    for (char letter : label) {
      const auto glyph = ModeGlyph(letter);
      for (int row = 0; row < 7; ++row) {
        for (int column = 0; column < 5; ++column) {
          if (glyph[static_cast<std::size_t>(row)] & (16 >> column)) {
            emit(x + column * scale, y + row * scale, scale, scale,
                 active ? 0xe6ffff : 0xc0cbd3);
          }
        }
      }
      x += 6 * scale;
    }
  }
}

}  // namespace camera_app

#endif  // EXAMPLES_CAMERA_APP_MODE_CONTROLS_H_
