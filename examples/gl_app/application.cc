#include "application.h"

#include <algorithm>
#include <chrono>
#include <cstdint>

#include <e32std.h>

#include "renderer.h"
#include "symbian/api/display/window_surface.h"
#include "symbian/api/time/frame_pacer.h"
#include "symbian/api/time/monotonic_clock.h"
#include "symbian/api/time/sleep.h"

namespace gl_app {
namespace {

int RunWindow(symbian::api::display::WindowSurface* absl_nonnull window) {
  Renderer renderer;
  if (!renderer.Open(window).ok()) {
    return KErrNotSupported;
  }
  renderer.Resize(TSize(window->size().width, window->size().height));
  const std::uint32_t refresh_rate =
      symbian::api::display::WindowSurface::PrimaryRefreshRateHz().value_or(60);
  symbian::api::time::FramePacer frame_pacer(refresh_rate);
  std::int64_t previous_frame =
      symbian::api::time::MonotonicClock::NowNanoseconds();
  std::int64_t frame_rate_start = previous_frame;
  std::uint32_t frame_count = 0;
  std::uint32_t frames_per_second = 0;
  float yaw = 0.65f;
  float pitch = 0.41f;
  int pointer_x = 0;
  int pointer_y = 0;
  bool dragging = false;
  bool paused = false;
  bool running = true;
  bool foreground = true;
  while (running) {
    for (int event_count = 0; event_count < 64; ++event_count) {
      auto event = window->PollInput();
      if (!event.ok()) {
        return KErrGeneral;
      }
      if (!event->has_value()) {
        break;
      }
      const symbian::api::display::WindowInput& input = **event;
      if (input.kind == symbian::api::display::WindowInputKind::kPointerDown ||
          input.kind == symbian::api::display::WindowInputKind::kPointerMove ||
          input.kind == symbian::api::display::WindowInputKind::kPointerUp) {
        if (paused) {
          const PausePanel::Action action = renderer.HandlePausePointer(input);
          if (action == PausePanel::Action::kResume) {
            paused = false;
            previous_frame =
                symbian::api::time::MonotonicClock::NowNanoseconds();
            frame_pacer.Reset();
          } else if (action == PausePanel::Action::kExit) {
            running = false;
          }
        } else if (input.kind ==
                   symbian::api::display::WindowInputKind::kPointerDown) {
          if (ExitButton::Bounds(
                  TSize(window->size().width, window->size().height))
                  .Contains(TPoint(input.x, input.y))) {
            renderer.HandlePointer(input);
          } else {
            dragging = true;
            pointer_x = input.x;
            pointer_y = input.y;
          }
        } else if (dragging) {
          yaw += static_cast<float>(input.x - pointer_x) * 0.012f;
          pitch += static_cast<float>(input.y - pointer_y) * 0.012f;
          pointer_x = input.x;
          pointer_y = input.y;
          if (input.kind ==
              symbian::api::display::WindowInputKind::kPointerUp) {
            dragging = false;
            previous_frame =
                symbian::api::time::MonotonicClock::NowNanoseconds();
          }
        } else {
          running = !renderer.HandlePointer(input);
        }
      } else if (input.kind ==
                     symbian::api::display::WindowInputKind::kKeyDown &&
                 input.key == symbian::api::display::WindowKey::kEscape) {
        paused = !paused;
        dragging = false;
        previous_frame = symbian::api::time::MonotonicClock::NowNanoseconds();
        frame_pacer.Reset();
      } else if (paused &&
                 input.kind ==
                     symbian::api::display::WindowInputKind::kKeyDown &&
                 input.key == symbian::api::display::WindowKey::kEnter) {
        paused = false;
        previous_frame = symbian::api::time::MonotonicClock::NowNanoseconds();
        frame_pacer.Reset();
      } else if (input.kind ==
                 symbian::api::display::WindowInputKind::kCloseRequested) {
        running = false;
      } else if (input.kind ==
                 symbian::api::display::WindowInputKind::kFocusLost) {
        foreground = false;
        paused = true;
        dragging = false;
      } else if (input.kind ==
                 symbian::api::display::WindowInputKind::kFocusGained) {
        foreground = true;
        previous_frame = symbian::api::time::MonotonicClock::NowNanoseconds();
        frame_rate_start = previous_frame;
        frame_count = 0;
        frame_pacer.Reset();
      } else if (input.kind ==
                 symbian::api::display::WindowInputKind::kDisplayChanged) {
        renderer.Resize(TSize(window->size().width, window->size().height));
      }
      if (!running) {
        break;
      }
    }
    if (!running) {
      break;
    }
    if (!foreground) {
      symbian::api::time::SleepFor(std::chrono::milliseconds(80));
      continue;
    }
    const std::int64_t now =
        symbian::api::time::MonotonicClock::NowNanoseconds();
    const std::int64_t delta =
        std::clamp<std::int64_t>(now - previous_frame, 0, 50000000);
    previous_frame = now;
    ++frame_count;
    if (now - frame_rate_start >= 1000000000) {
      frames_per_second =
          static_cast<std::uint32_t>(static_cast<std::int64_t>(frame_count) *
                                     1000000000 / (now - frame_rate_start));
      frame_count = 0;
      frame_rate_start = now;
    }
    if (!paused && !dragging) {
      yaw += static_cast<float>(delta) * 0.00000000065f;
      pitch += static_cast<float>(delta) * 0.0000000004095f;
    }
    if (!renderer.Draw(yaw, pitch, paused, frames_per_second).ok()) {
      return KErrGeneral;
    }
    symbian::api::time::SleepFor(frame_pacer.NextDelayNanoseconds());
  }
  return KErrNone;
}

}  // namespace

int RunApplication() {
  symbian::api::display::WindowSurface window;
  if (!window.Open("Symbian GL Cube").ok()) {
    return KErrNotSupported;
  }
  if (window.size().width < 160 || window.size().height < 240) {
    return KErrNotSupported;
  }
  return RunWindow(&window);
}

}  // namespace gl_app
