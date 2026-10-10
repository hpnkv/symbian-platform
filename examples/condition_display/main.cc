// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>

#include <absl/base/nullability.h>

#include "absl/status/status.h"
#include "symbian/api/display/window_surface.h"
#include "symbian/api/power/power.h"
#include "symbian/api/system/debug_log.h"
#include "symbian/api/time/monotonic_clock.h"
#include "symbian/api/time/sleep.h"

namespace {

namespace display = symbian::api::display;
namespace time = symbian::api::time;

constexpr std::int64_t kPhaseNanoseconds = 4000000000LL;
constexpr std::int64_t kPollNanoseconds = 25000000LL;
constexpr std::int64_t kAwakePulseNanoseconds = 1000000000LL;
constexpr std::array<std::uint16_t, 8> kColors{
    0xf800,  // Red.
    0x07e0,  // Green.
    0x001f,  // Blue.
    0xffe0,  // Red + green.
    0x07ff,  // Green + blue.
    0xf81f,  // Red + blue.
    0xffff,  // Red + green + blue.
    0x0000,  // Black.
};

struct FillColor {
  std::uint16_t rgb565;

  absl::Status Write(display::Rgb565Frame frame) const {
    const auto color = rgb565;
    for (int y = 0; y < frame.size.height; ++y) {
      std::byte* absl_nonnull row =
          frame.pixels.data() +
          static_cast<std::size_t>(y) * frame.pitch_bytes;
      for (int x = 0; x < frame.size.width; ++x) {
        row[x * 2] = static_cast<std::byte>(color & 0xff);
        row[x * 2 + 1] = static_cast<std::byte>(color >> 8);
      }
    }
    return absl::OkStatus();
  }
};

absl::Status Run() {
  display::WindowSurface window;
  absl::Status status = window.Open("Condition display");
  if (!status.ok()) {
    return status;
  }
  status = window.SetAutomaticOrientation(true);
  if (!status.ok()) {
    return status;
  }
  bool running = true;
  bool foreground = true;
  bool redraw = true;
  std::int64_t active_time = 0;
  std::int64_t last_tick = time::MonotonicClock::NowNanoseconds();
  std::int64_t last_awake_pulse = last_tick - kAwakePulseNanoseconds;
  std::size_t current_color = 0;
  while (running) {
    const std::int64_t now = time::MonotonicClock::NowNanoseconds();
    if (foreground && now > last_tick) {
      active_time += now - last_tick;
    }
    last_tick = now;
    for (int i = 0; i < 32; ++i) {
      auto event = window.PollInput();
      if (!event.ok()) {
        return event.status();
      }
      if (!event->has_value()) {
        break;
      }
      switch ((**event).kind) {
        case display::WindowInputKind::kCloseRequested:
          running = false;
          break;
        case display::WindowInputKind::kKeyDown:
          if ((**event).key == display::WindowKey::kEscape) {
            running = false;
          }
          break;
        case display::WindowInputKind::kFocusLost:
          foreground = false;
          break;
        case display::WindowInputKind::kFocusGained:
          foreground = true;
          last_awake_pulse = now - kAwakePulseNanoseconds;
          redraw = true;
          break;
        case display::WindowInputKind::kDisplayChanged:
          redraw = true;
          break;
        default:
          break;
      }
    }
    if (!running) {
      break;
    }
    if (foreground && now - last_awake_pulse >= kAwakePulseNanoseconds) {
      symbian::api::power::ResetInactivityTimer();
      last_awake_pulse = now;
    }
    const std::size_t color = static_cast<std::size_t>(
        active_time / kPhaseNanoseconds % kColors.size());
    if (color != current_color) {
      current_color = color;
      redraw = true;
    }
    if (foreground && redraw) {
      FillColor fill{kColors[current_color]};
      status = window.UpdateRgb565Frame(
          {.write = [&fill](display::Rgb565Frame frame) {
             return fill.Write(frame);
           }});
      if (!status.ok()) {
        return status;
      }
      status = window.Present();
      if (!status.ok()) {
        return status;
      }
      redraw = false;
    }
    time::SleepFor(std::chrono::nanoseconds(kPollNanoseconds));
  }
  return absl::OkStatus();
}

}  // namespace

int main() {
  absl::Status result = Run();
  if (!result.ok()) {
    symbian::api::system::DebugLog(result.ToString());
    return 1;
  }
  return 0;
}
