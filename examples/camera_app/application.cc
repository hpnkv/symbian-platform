// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "application.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

#include "capture_preferences.h"
#include "symbian/api/camera/camera.h"
#include "symbian/api/camera/camera_stream.h"
#include "symbian/api/camera/frame.h"
#include "symbian/api/display/window_surface.h"
#include "symbian/api/system/debug_log.h"
#include "symbian/api/time/frame_pacer.h"
#include "symbian/api/time/monotonic_clock.h"
#include "symbian/api/time/sleep.h"
#include "trace.h"

#if defined(SYMBIAN_CAMERA_GPU_PROBE)
#include "gpu_probe.h"
#endif

namespace camera_app {
namespace {

namespace camera = symbian::api::camera;
namespace display = symbian::api::display;
namespace time = symbian::api::time;

constexpr int kSourceWidth = 64;
constexpr int kSourceHeight = 48;
std::array<std::byte, kSourceWidth * kSourceHeight * 4> source_pixels;

void FillSource(std::uint64_t sequence) {
  constexpr std::array<std::array<unsigned char, 3>, 6> colors{{
      {255, 48, 48},
      {255, 210, 48},
      {48, 210, 64},
      {48, 210, 255},
      {68, 80, 255},
      {220, 64, 220},
  }};
  const int marker = static_cast<int>(sequence % kSourceWidth);
  for (int y = 0; y < kSourceHeight; ++y) {
    for (int x = 0; x < kSourceWidth; ++x) {
      const auto& color = colors[(x * 6) / kSourceWidth];
      const bool highlight =
          x == marker || y == marker * kSourceHeight / kSourceWidth;
      const std::size_t offset =
          static_cast<std::size_t>(y * kSourceWidth + x) * 4;
      source_pixels[offset] =
          static_cast<std::byte>(highlight ? 255 : color[0]);
      source_pixels[offset + 1] =
          static_cast<std::byte>(highlight ? 255 : color[1]);
      source_pixels[offset + 2] =
          static_cast<std::byte>(highlight ? 255 : color[2]);
      source_pixels[offset + 3] = std::byte{255};
    }
  }
}

void DrawStripe(const display::Rgb565Frame& frame, int first_x, int last_x,
                unsigned short color) {
  const int rows = std::min(frame.size.height, 12);
  first_x = std::clamp(first_x, 0, frame.size.width);
  last_x = std::clamp(last_x, 0, frame.size.width);
  for (int y = 0; y < rows; ++y) {
    for (int x = first_x; x < last_x; ++x) {
      std::byte* absl_nonnull pixel =
          frame.pixels.data() + y * frame.pitch_bytes + x * 2;
      pixel[0] = static_cast<std::byte>(color & 255);
      pixel[1] = static_cast<std::byte>(color >> 8);
    }
  }
}

class SoftwareFrameConsumer final {
 public:
  SoftwareFrameConsumer(display::WindowSurface* absl_nonnull window,
                        camera::ResampleFilter* absl_nonnull filter,
                        display::DisplayRotation opening_rotation)
      : window_(window), filter_(filter), opening_rotation_(opening_rotation) {}

  camera::FrameConsumer Borrow() {
    return {.consume = [this](const camera::FrameView& source) {
      return Consume(source);
    }};
  }

  absl::Status Consume(const camera::FrameView& source) {
    source_ = &source;
    absl::Status result = window_->UpdateRgb565Frame(
        {.write = [this](display::Rgb565Frame frame) { return Write(frame); }});
    source_ = nullptr;
    return result;
  }

  absl::Status Write(display::Rgb565Frame frame) {
    const camera::FrameRotation rotation =
        PresentationRotation(window_->rotation(), opening_rotation_);
    const bool quarter_turn = rotation == camera::FrameRotation::kClockwise90 ||
                              rotation == camera::FrameRotation::kClockwise270;
    auto fitted = camera::CenteredAspectFit(
        quarter_turn ? source_->layout.height : source_->layout.width,
        quarter_turn ? source_->layout.width : source_->layout.height,
        frame.size.width, frame.size.height);
    if (!fitted.ok()) {
      return fitted.status();
    }
    std::fill(frame.pixels.begin(), frame.pixels.end(), std::byte{0});
    camera::MutableFrameView output;
    output.layout = {.width = fitted->width,
                     .height = fitted->height,
                     .format = camera::PixelFormat::kRgb565,
                     .stride_bytes = {frame.pitch_bytes, 0, 0}};
    const std::size_t offset =
        static_cast<std::size_t>(fitted->y) * frame.pitch_bytes +
        static_cast<std::size_t>(fitted->x) * 2;
    output.planes[0] = frame.pixels.subspan(offset);
    return camera::TransformFrame(*source_, output, *filter_, nullptr,
                                  rotation);
  }

 private:
  display::WindowSurface* absl_nonnull window_;
  camera::ResampleFilter* absl_nonnull filter_;
  display::DisplayRotation opening_rotation_;
  const camera::FrameView* absl_nullable source_ = nullptr;
};

int RunWindow(display::WindowSurface* absl_nonnull window) {
  Trace("software: discover begin");
  const auto inventory = camera::DiscoverCameras();
  Trace(inventory.ok() ? "software: discover done"
                       : "software: discover error");
  const bool discovery_ok = inventory.ok();
  const bool camera_present = inventory.ok() && inventory->available_count > 0;
  camera::CameraStream stream;
  const display::DisplayRotation opening_rotation = window->rotation();
  const auto preferences =
      CapturePreferences(window->size().height >= window->size().width);
  if (camera_present) {
    Trace("software: stream open begin");
  }
  if (absl::Status opened =
          camera_present ? stream.Open(0, preferences) : absl::OkStatus();
      !opened.ok()) {
    RecordFailure(opened);
    return 1;
  }
  bool live = camera_present;
  Trace(live ? "software: stream open done" : "software: stream unavailable");
  symbian::api::system::DebugLog(live ? "camera capture opening"
                                      : "camera capture unavailable");
  std::int64_t opening_time = time::MonotonicClock::NowNanoseconds();
  bool received_frame = false;
  bool capturing = live;
  std::optional<display::Rgb565Frame> frame;
  if (!live) {
    auto created = window->CreateRgb565Frame();
    if (!created.ok()) {
      RecordFailure(created.status());
      return 1;
    }
    frame = *created;
  }
  const std::uint32_t refresh =
      display::WindowSurface::PrimaryRefreshRateHz().value_or(30);
  time::FramePacer pacer(std::min(refresh, std::uint32_t{30}));
  camera::ResampleFilter filter = camera::ResampleFilter::kNearest;
  SoftwareFrameConsumer consumer(window, &filter, opening_rotation);
  std::uint64_t sequence = 0;
  bool foreground = true;
  bool running = true;
  while (running) {
    for (int count = 0; count < 64; ++count) {
      auto input = window->PollInput();
      if (!input.ok()) {
        Trace("software: input error");
        RecordFailure(input.status());
        return 1;
      }
      if (!input->has_value()) {
        break;
      }
      switch ((**input).kind) {
        case display::WindowInputKind::kPointerDown:
          filter = filter == camera::ResampleFilter::kNearest
                       ? camera::ResampleFilter::kBilinear
                       : camera::ResampleFilter::kNearest;
          break;
        case display::WindowInputKind::kKeyDown:
          if ((**input).key == display::WindowKey::kEscape) {
            Trace("software: escape key");
            running = false;
          } else if ((**input).key == display::WindowKey::kSelect ||
                     (**input).key == display::WindowKey::kEnter) {
            filter = filter == camera::ResampleFilter::kNearest
                         ? camera::ResampleFilter::kBilinear
                         : camera::ResampleFilter::kNearest;
          }
          break;
        case display::WindowInputKind::kCloseRequested:
          Trace("software: close requested");
          running = false;
          break;
        case display::WindowInputKind::kFocusLost:
          Trace("software: focus lost");
          foreground = false;
          break;
        case display::WindowInputKind::kFocusGained:
          Trace("software: focus gained");
          foreground = true;
          if (!live && (frame->size.width != window->size().width ||
                        frame->size.height != window->size().height)) {
            auto created = window->CreateRgb565Frame();
            if (!created.ok()) {
              RecordFailure(created.status());
              return 1;
            }
            frame = *created;
          }
          pacer.Reset();
          break;
        case display::WindowInputKind::kDisplayChanged:
          Trace("software: display changed");
          if (!live) {
            window->DestroyFrame();
            auto created = window->CreateRgb565Frame();
            if (!created.ok()) {
              RecordFailure(created.status());
              return 1;
            }
            frame = *created;
          }
          break;
        default:
          break;
      }
    }
    if (!running) {
      break;
    }
    if (!foreground) {
      if (capturing) {
        Trace("software: capture suspend begin");
        stream.Close();
        Trace("software: capture suspend done");
        capturing = false;
        received_frame = false;
      }
      time::SleepFor(std::chrono::milliseconds(80));
      continue;
    }
    if (live && !capturing) {
      Trace("software: capture resume begin");
      if (absl::Status resumed = stream.Open(0, preferences); !resumed.ok()) {
        RecordFailure(resumed);
        return 1;
      }
      Trace("software: capture resume done");
      capturing = true;
      opening_time = time::MonotonicClock::NowNanoseconds();
    }
    const std::int64_t now = time::MonotonicClock::NowNanoseconds();
    bool captured = false;
    if (live) {
      if (sequence < 8) {
        Trace("software: poll begin");
      }
      auto polled = stream.PollScoped(consumer.Borrow());
      if (sequence < 8) {
        Trace(polled.ok() ? "software: poll done" : "software: poll error");
      }
      if (!polled.ok()) {
        RecordFailure(polled.status());
        return 1;
      } else if (*polled) {
        if (!received_frame) {
          Trace("software: first frame");
        }
        captured = true;
        received_frame = true;
      }
    }
    if (live && !received_frame && now - opening_time > 5000000000LL) {
      symbian::api::system::DebugLog("camera capture timed out");
      Trace("camera capture timed out");
      RecordFailure(absl::DeadlineExceededError("camera capture timed out"));
      return 1;
    }
    if (live && !captured) {
      time::SleepFor(pacer.NextDelayNanoseconds());
      continue;
    }
    if (!live) {
      FillSource(sequence);
      camera::FrameView source;
      source.layout = {.width = kSourceWidth,
                       .height = kSourceHeight,
                       .format = camera::PixelFormat::kRgba8888,
                       .stride_bytes = {kSourceWidth * 4, 0, 0}};
      source.identity = {.sequence = sequence,
                         .capture_time_ns = now,
                         .clock = camera::ClockDomain::kMonotonic};
      source.planes[0] = source_pixels;
      camera::MutableFrameView output;
      output.layout = {.width = frame->size.width,
                       .height = frame->size.height,
                       .format = camera::PixelFormat::kRgb565,
                       .stride_bytes = {frame->pitch_bytes, 0, 0}};
      output.planes[0] = frame->pixels;
      if (absl::Status transformed =
              camera::TransformFrame(source, output, filter);
          !transformed.ok()) {
        Trace("software: transform error");
        RecordFailure(transformed);
        return 1;
      }
      const int third = frame->size.width / 3;
      DrawStripe(*frame, 0, third, !discovery_ok ? 0xf800 : 0xffe0);
      DrawStripe(*frame, third, third * 2,
                 filter == camera::ResampleFilter::kNearest ? 0x001f : 0x07ff);
      DrawStripe(*frame, third * 2, frame->size.width, 0x7bef);
    }
    if (absl::Status presented = window->Present(); !presented.ok()) {
      Trace("software: present error");
      RecordFailure(presented);
      return 1;
    }
    if (sequence < 8) {
      Trace("software: present done");
    }
    ++sequence;
    time::SleepFor(pacer.NextDelayNanoseconds());
  }
  Trace("software: normal exit");
  return 0;
}

}  // namespace

int Run() {
  OpenTrace();
#if defined(SYMBIAN_CAMERA_GPU_PROBE)
  Trace("GPU: start");
#else
  Trace("software: start");
#endif
  display::WindowSurface window;
  if (absl::Status opened = window.Open("Camera capture"); !opened.ok()) {
    Trace("software: window open error");
    RecordFailure(opened);
    CloseTrace();
    return 1;
  }
  Trace("software: window open done");
  if (absl::Status orientation = window.SetAutomaticOrientation(true);
      !orientation.ok()) {
    RecordFailure(orientation);
    CloseTrace();
    return 1;
  }
#if defined(SYMBIAN_CAMERA_GPU_PROBE)
  const int result = RunGpuWindow(&window);
#else
  const int result = RunWindow(&window);
#endif
  Trace(result == 0 ? "app: return success" : "app: return failure");
  Trace("app: window close begin");
  window.Close();
  Trace("app: window close returned");
  CloseTrace();
  return result;
}

}  // namespace camera_app
