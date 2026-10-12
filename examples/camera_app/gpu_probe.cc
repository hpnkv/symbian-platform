// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "gpu_probe.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include <absl/status/status_macros.h>

#include "capture_preferences.h"
#include "mode_controls.h"
#include "symbian/api/camera/camera.h"
#include "symbian/api/camera/camera_stream.h"
#include "symbian/api/camera/frame.h"
#include "symbian/api/camera/gles2_frame_backend.h"
#include "symbian/api/display/gles2_texture.h"
#include "symbian/api/display/gles_rect_batch.h"
#include "symbian/api/display/gles_window_context.h"
#include "symbian/api/display/window_surface.h"
#include "symbian/api/storage/storage.h"
#include "symbian/api/system/debug_log.h"
#include "symbian/api/time/frame_pacer.h"
#include "symbian/api/time/monotonic_clock.h"
#include "symbian/api/time/sleep.h"
#include "trace.h"
#include "viewfinder_presentation.h"

namespace camera_app {
namespace {

namespace camera = symbian::api::camera;
namespace display = symbian::api::display;
namespace time = symbian::api::time;

constexpr std::array<std::byte, 8> kPixels{
    std::byte{31}, std::byte{0},   std::byte{224}, std::byte{7},
    std::byte{0},  std::byte{248}, std::byte{255}, std::byte{255}};

void AppendDecimal(int value, std::string* absl_nonnull output) {
  char digits[12];
  int count = 0;
  unsigned int remaining = static_cast<unsigned int>(value);
  do {
    digits[count++] = static_cast<char>('0' + remaining % 10);
    remaining /= 10;
  } while (remaining != 0);
  while (count > 0) {
    output->push_back(digits[--count]);
  }
}

void TraceGeometry(display::WindowSize window_size,
                   display::WindowSize surface_size,
                   display::DisplayRotation rotation) {
  std::string detail("GPU: geometry window=");
  AppendDecimal(window_size.width, &detail);
  detail.push_back('x');
  AppendDecimal(window_size.height, &detail);
  detail.append(" egl=");
  AppendDecimal(surface_size.width, &detail);
  detail.push_back('x');
  AppendDecimal(surface_size.height, &detail);
  detail.append(" rotation=");
  AppendDecimal(static_cast<int>(rotation), &detail);
  Trace(detail);
}

display::TextureRotation ToTextureRotation(camera::FrameRotation rotation) {
  switch (rotation) {
    case camera::FrameRotation::k0:
      return display::TextureRotation::k0;
    case camera::FrameRotation::kClockwise90:
      return display::TextureRotation::kClockwise90;
    case camera::FrameRotation::k180:
      return display::TextureRotation::k180;
    case camera::FrameRotation::kClockwise270:
      return display::TextureRotation::kClockwise270;
  }
  return display::TextureRotation::k0;
}

// Draw directly into the EGL surface; a Window Server bitmap overlay would
// compete with swaps and could reappear over the first camera frame.
absl::Status DrawWaiting(display::Gles2TexturePresenter* absl_nonnull presenter,
                         display::GlesRectBatch* absl_nonnull shapes,
                         display::WindowSize size, std::int64_t now) {
  ABSL_RETURN_IF_ERROR(
      presenter->Clear(size.width, size.height, 0.035f, 0.047f, 0.063f));
  shapes->Begin(size.width, size.height);
  const int unit = std::max(1, std::min(size.width, size.height) / 120);
  const int cx = size.width / 2;
  const int cy = size.height / 2 - 6 * unit;
  // Camera body, raised shutter housing, and a circular lens.
  shapes->AddRect(cx - 16 * unit, cy - 10 * unit, 32 * unit, 22 * unit, 0.52f,
                  0.60f, 0.66f);
  shapes->AddRect(cx - 8 * unit, cy - 14 * unit, 16 * unit, 4 * unit, 0.52f,
                  0.60f, 0.66f);
  shapes->AddRect(cx - 14 * unit, cy - 8 * unit, 28 * unit, 18 * unit, 0.035f,
                  0.047f, 0.063f);
  const int radius = 7 * unit;
  for (int y = -radius; y <= radius; ++y) {
    int x = radius;
    while (x * x + y * y > radius * radius) {
      --x;
    }
    shapes->AddRect(cx - x, cy + y, 2 * x + 1, 1, 0.26f, 0.77f, 0.80f);
  }
  const int inner = 5 * unit;
  for (int y = -inner; y <= inner; ++y) {
    int x = inner;
    while (x * x + y * y > inner * inner) {
      --x;
    }
    shapes->AddRect(cx - x, cy + y, 2 * x + 1, 1, 0.035f, 0.047f, 0.063f);
  }
  const int active = static_cast<int>((now / 250000000) % 3);
  for (int dot = 0; dot < 3; ++dot) {
    const bool bright = dot == active;
    shapes->AddRect(cx + (dot - 1) * 7 * unit - unit, cy + 23 * unit, 3 * unit,
                    3 * unit, bright ? 0.26f : 0.16f, bright ? 0.77f : 0.22f,
                    bright ? 0.80f : 0.27f);
  }
  shapes->Draw();
  return absl::OkStatus();
}

void DrawControls(display::GlesRectBatch* absl_nonnull shapes,
                  display::WindowSize size, display::DisplayRotation rotation,
                  ViewfinderMode selected) {
  shapes->Begin(size.width, size.height);
  DrawModeButtons(
      size, rotation, selected,
      [shapes](int x, int y, int width, int height, std::uint32_t color) {
        shapes->AddRect(x, y, width, height, ((color >> 16) & 255) / 255.0f,
                        ((color >> 8) & 255) / 255.0f, (color & 255) / 255.0f);
      });
  shapes->Draw();
}

void ToggleFilter(camera::ResampleFilter* absl_nonnull filter) {
  *filter = *filter == camera::ResampleFilter::kNearest
                ? camera::ResampleFilter::kBilinear
                : camera::ResampleFilter::kNearest;
}

class GpuFrameConsumer final {
 public:
  GpuFrameConsumer(display::Gles2Texture* absl_nonnull texture,
                   camera::FrameBackend* absl_nonnull backend,
                   std::uintptr_t device)
      : texture_(texture), backend_(backend), device_(device) {}

  camera::FrameConsumer Borrow() {
    return {.consume = [this](const camera::FrameView& frame) {
      return Consume(frame);
    }};
  }

  absl::Status Consume(const camera::FrameView& frame) {
    if (!has_frame_) {
      std::string detail("GPU: source ");
      AppendDecimal(frame.layout.width, &detail);
      detail.push_back('x');
      AppendDecimal(frame.layout.height, &detail);
      detail.append(" format=");
      AppendDecimal(static_cast<int>(frame.layout.format), &detail);
      Trace(detail);
      content_ = {0, 0, frame.layout.width, frame.layout.height};
      std::string bounds("GPU: content x=");
      AppendDecimal(content_.x, &bounds);
      bounds.append(" width=");
      AppendDecimal(content_.width, &bounds);
      Trace(bounds);
#if defined(SYMBIAN_CAMERA_DIAGNOSTIC_FRAME)
      if (!frame.planes[0].empty()) {
        if (auto dump = symbian::api::storage::WritableFile::Open(
                u"E:\\Others\\camera_gl_first_frame.raw",
                symbian::api::storage::WriteMode::kReplaceExisting);
            dump.ok()) {
          absl::Status saved = dump->WriteAt(0, frame.planes[0]);
          if (saved.ok()) {
            saved = dump->Flush();
          }
          Trace(saved.ok() ? "GPU: raw frame saved"
                           : "GPU: raw frame save failed");
        } else {
          Trace("GPU: raw frame open failed");
        }
      }
#endif
    }
    if (texture_->width() != frame.layout.width ||
        texture_->height() != frame.layout.height) {
      ABSL_RETURN_IF_ERROR(
          texture_->Resize(frame.layout.width, frame.layout.height));
      Trace("GPU: source texture resized");
    }
    const bool bgrx = frame.layout.format == camera::PixelFormat::kBgrx8888;
    camera::MutableFrameView output;
    output.layout = {.width = texture_->width(),
                     .height = texture_->height(),
                     .format = bgrx ? camera::PixelFormat::kBgrx8888
                                    : camera::PixelFormat::kRgba8888};
    output.memory = camera::MemoryKind::kGles2Texture;
    output.handle = texture_->handle();
    output.device = device_;
    ABSL_RETURN_IF_ERROR(camera::TransformFrame(
        frame, output, camera::ResampleFilter::kNearest, backend_));
    raw_bgrx_ = bgrx;
    has_frame_ = true;
    return absl::OkStatus();
  }

  bool has_frame() const { return has_frame_; }

  bool raw_bgrx() const { return raw_bgrx_; }

  camera::FrameRect content() const { return content_; }

  void Reset() {
    has_frame_ = false;
    content_ = {};
  }

 private:
  display::Gles2Texture* absl_nonnull texture_;
  camera::FrameBackend* absl_nonnull backend_;
  std::uintptr_t device_;
  bool has_frame_ = false;
  bool raw_bgrx_ = false;
  camera::FrameRect content_;
};

}  // namespace

absl::Status RunGpuWindow(display::WindowSurface* absl_nonnull window) {
  ABSL_ASSIGN_OR_RETURN(
      auto context,
      display::GlesWindowContext::Create(
          window, 2,
          {.red_bits = 8, .green_bits = 8, .blue_bits = 8, .depth_bits = 0}));
  Trace("GPU: context open done");
  ABSL_ASSIGN_OR_RETURN(auto source_texture,
                        display::Gles2Texture::Create(2, 2));
  ABSL_ASSIGN_OR_RETURN(auto output_texture,
                        display::Gles2Texture::Create(2, 2));
  ABSL_ASSIGN_OR_RETURN(auto presenter,
                        display::Gles2TexturePresenter::Create());
  display::WindowSize window_size = window->size();
  ABSL_ASSIGN_OR_RETURN(display::WindowSize size, context.SurfaceSize());
  TraceGeometry(window_size, size, window->rotation());
  Trace("GPU: texture and presenter done");
  ABSL_ASSIGN_OR_RETURN(auto waiting_shapes, display::GlesRectBatch::Create());
  ViewfinderPresentation viewfinder(window->rotation(), window_size);
  ABSL_RETURN_IF_ERROR(DrawWaiting(&presenter, &waiting_shapes, size,
                                   time::MonotonicClock::NowNanoseconds()));
  DrawControls(&waiting_shapes, size, window->rotation(), viewfinder.mode());
  ABSL_RETURN_IF_ERROR(context.Swap());
  Trace("GPU: waiting UI presented");
  Trace("GPU: discover begin");
  const auto inventory = camera::DiscoverCameras();
  Trace(inventory.ok() ? "GPU: discover done" : "GPU: discover error");
  const bool discovered = inventory.ok();
  const bool present = inventory.ok() && inventory->available_count > 0;
  camera::CameraStream stream;
  Trace("GPU: mode rear");
  if (present) {
    Trace("GPU: stream open begin");
  }
  if (present) {
    ABSL_RETURN_IF_ERROR(stream.Open(0, viewfinder.preferences()));
  }
  const bool live = present;
  Trace(live ? "GPU: stream open done" : "GPU: stream unavailable");
  symbian::api::system::DebugLog(live ? "camera GPU capture opening"
                                      : "camera GPU capture unavailable");
  std::int64_t opening_time = time::MonotonicClock::NowNanoseconds();
  bool received_frame = false;
  bool capturing = live;

  const std::uintptr_t device = reinterpret_cast<std::uintptr_t>(&context);
  camera::Gles2FrameBackend backend(device,
                                    camera::Gles2ContextUse::kExclusive);
  camera::FrameBackend backend_view = backend.Borrow();
  GpuFrameConsumer consumer(&output_texture, &backend_view, device);
  const std::uint32_t refresh =
      display::WindowSurface::PrimaryRefreshRateHz().value_or(30);
  time::FramePacer pacer(std::min(refresh, std::uint32_t{30}));
  camera::ResampleFilter filter = camera::ResampleFilter::kNearest;
  bool foreground = true;
  bool running = true;
  bool first_frame = true;
  bool surface_refresh_pending = false;
  bool redraw_pending = false;
  bool restart_pending = false;
  const auto select_mode = [&](ViewfinderMode mode) {
    if (mode == viewfinder.mode()) {
      return;
    }
    viewfinder.Configure(mode, window->rotation(), window->size());
    restart_pending = true;
    redraw_pending = true;
    Trace(mode == ViewfinderMode::kRearCamera ? "GPU: mode rear"
                                              : "GPU: mode rotate");
  };
  std::uint64_t sequence = 0;
  std::uint64_t poll_count = 0;
  std::uint64_t presented_frames = 0;
  std::int64_t presented_interval_start_ns =
      time::MonotonicClock::NowNanoseconds();
  while (running) {
    ++poll_count;
    for (int count = 0; count < 64; ++count) {
      ABSL_ASSIGN_OR_RETURN(auto input, window->PollInput());
      if (!input.has_value()) {
        break;
      }
      switch (input->kind) {
        case display::WindowInputKind::kPointerDown:
          if (auto selected = HitModeButton(window->size(), window->rotation(),
                                            input->x, input->y)) {
            select_mode(*selected);
          } else {
            ToggleFilter(&filter);
          }
          break;
        case display::WindowInputKind::kKeyDown:
          if (input->key == display::WindowKey::kEscape) {
            Trace("GPU: escape key");
            running = false;
          } else if (input->key == display::WindowKey::kLeft) {
            select_mode(ViewfinderMode::kRearCamera);
          } else if (input->key == display::WindowKey::kRight) {
            select_mode(ViewfinderMode::kRotateView);
          } else if (input->key == display::WindowKey::kSelect ||
                     input->key == display::WindowKey::kEnter) {
            ToggleFilter(&filter);
          }
          break;
        case display::WindowInputKind::kCloseRequested:
          Trace("GPU: close requested");
          running = false;
          break;
        case display::WindowInputKind::kFocusLost:
          Trace("GPU: focus lost");
          foreground = false;
          break;
        case display::WindowInputKind::kFocusGained:
          Trace("GPU: focus gained");
          foreground = true;
          if (window->size().width != window_size.width ||
              window->size().height != window_size.height) {
            window_size = window->size();
            surface_refresh_pending = true;
          }
          redraw_pending = true;
          pacer.Reset();
          break;
        case display::WindowInputKind::kDisplayChanged: {
          Trace("GPU: display changed");
          if (const display::WindowSize updated = window->size();
              updated.width != window_size.width ||
              updated.height != window_size.height) {
            window_size = updated;
            surface_refresh_pending = true;
          }
          redraw_pending = true;
          break;
        }
        default:
          break;
      }
    }
    if (!running) {
      break;
    }
    if (restart_pending) {
      stream.Close();
      capturing = false;
      received_frame = false;
      consumer.Reset();
      first_frame = true;
      restart_pending = false;
    }
    if (!foreground) {
      if (capturing) {
        Trace("GPU: capture suspend begin");
        stream.Close();
        Trace("GPU: capture suspend done");
        capturing = false;
        received_frame = false;
        consumer.Reset();
      }
      time::SleepFor(std::chrono::milliseconds(80));
      continue;
    }
    if (surface_refresh_pending) {
      Trace("GPU: surface refresh begin");
      ABSL_RETURN_IF_ERROR(context.RefreshSurface());
      Trace("GPU: surface refresh done");
      ABSL_ASSIGN_OR_RETURN(size, context.SurfaceSize());
      TraceGeometry(window_size, size, window->rotation());
      surface_refresh_pending = false;
      redraw_pending = true;
    }
    if (live && !capturing) {
      Trace("GPU: capture resume begin");
      ABSL_RETURN_IF_ERROR(stream.Open(0, viewfinder.preferences()));
      Trace("GPU: capture resume done");
      capturing = true;
      opening_time = time::MonotonicClock::NowNanoseconds();
      first_frame = true;
    }
    bool new_frame = false;
    if (live) {
      if (poll_count < 8) {
        Trace("GPU: poll begin");
      }
      ABSL_ASSIGN_OR_RETURN(const bool polled,
                            stream.PollScoped(consumer.Borrow()));
      if (poll_count < 8) {
        Trace("GPU: poll done");
      }
      if (polled) {
        new_frame = true;
        if (!received_frame) {
          Trace("GPU: first frame");
          presented_frames = 0;
          presented_interval_start_ns = time::MonotonicClock::NowNanoseconds();
        }
        received_frame = true;
      }
    }
    const std::int64_t now = time::MonotonicClock::NowNanoseconds();
    if (live && !received_frame && now - opening_time > 5000000000LL) {
      symbian::api::system::DebugLog("camera GPU capture timed out");
      Trace("camera GPU capture timed out");
      return absl::DeadlineExceededError("camera GPU capture timed out");
    }
    if (live && !received_frame) {
      if (poll_count < 8) {
        Trace("GPU: wait frame present begin");
      }
      ABSL_RETURN_IF_ERROR(DrawWaiting(&presenter, &waiting_shapes, size, now));
      DrawControls(&waiting_shapes, size, window->rotation(),
                   viewfinder.mode());
      ABSL_RETURN_IF_ERROR(context.Swap());
      if (poll_count < 8) {
        Trace("GPU: wait frame present done");
      }
      time::SleepFor(pacer.NextDelayNanoseconds());
      continue;
    }
    if (live && !new_frame && !redraw_pending) {
      time::SleepFor(std::chrono::milliseconds(5));
      continue;
    }
    camera::MutableFrameView output;
    output.layout = {.width = output_texture.width(),
                     .height = output_texture.height(),
                     .format = camera::PixelFormat::kRgba8888};
    output.memory = camera::MemoryKind::kGles2Texture;
    output.handle = output_texture.handle();
    output.device = device;

    if (!live) {
      if (first_frame) {
        symbian::api::system::DebugLog("camera GPU synthetic upload start");
      }
      camera::FrameView memory;
      memory.layout = {.width = 2,
                       .height = 2,
                       .format = camera::PixelFormat::kBgr565,
                       .stride_bytes = {4, 0, 0}};
      memory.identity = {.sequence = sequence,
                         .capture_time_ns = now,
                         .clock = camera::ClockDomain::kMonotonic};
      memory.planes[0] = kPixels;
      camera::MutableFrameView upload;
      upload.layout = {
          .width = 2, .height = 2, .format = camera::PixelFormat::kRgba8888};
      upload.memory = camera::MemoryKind::kGles2Texture;
      upload.handle = source_texture.handle();
      upload.device = device;
      camera::FrameView source;
      source.layout = upload.layout;
      source.memory = camera::MemoryKind::kGles2Texture;
      source.handle = source_texture.handle();
      source.device = device;
      ABSL_RETURN_IF_ERROR(camera::TransformFrame(
          memory, upload, camera::ResampleFilter::kNearest, &backend_view));
      if (first_frame) {
        symbian::api::system::DebugLog("camera GPU synthetic upload done");
      }
      ABSL_RETURN_IF_ERROR(
          camera::TransformFrame(source, output, filter, &backend_view));
    }
    if (first_frame) {
      symbian::api::system::DebugLog("camera GPU transform done");
    }
    const float red = !live && (!TraceReady() || !discovered) ? 1.0f : 0.0f;
    const float green = !live && discovered ? 1.0f : 0.0f;
    ABSL_RETURN_IF_ERROR(
        presenter.Clear(size.width, size.height, red, green, 0.0f));
    if (first_frame) {
      symbian::api::system::DebugLog("camera GPU clear done");
    }
    ViewfinderGeometry geometry{
        {0, 0, output_texture.width(), output_texture.height()},
        camera::FrameRotation::k0,
        {output_texture.width(), output_texture.height()}};
    if (live) {
      ABSL_ASSIGN_OR_RETURN(
          geometry,
          viewfinder.Describe(output_texture.width(), output_texture.height(),
                              window->rotation()));
    }
    const auto image_rotation = ToTextureRotation(geometry.orientation);
    const auto content = geometry.crop;
    const int image_width = geometry.oriented_size.width;
    const int image_height = geometry.oriented_size.height;
    ABSL_ASSIGN_OR_RETURN(
        auto viewport, display::AspectFillViewport(size.width, size.height,
                                                   image_width, image_height));
    if (first_frame || redraw_pending) {
      std::string viewport_detail("GPU: fill viewport=");
      AppendDecimal(viewport.width, &viewport_detail);
      viewport_detail.push_back('x');
      AppendDecimal(viewport.height, &viewport_detail);
      viewport_detail.append(" image=");
      AppendDecimal(image_width, &viewport_detail);
      viewport_detail.push_back('x');
      AppendDecimal(image_height, &viewport_detail);
      Trace(viewport_detail);
      std::string bounds("GPU: view crop=");
      for (int value : {content.x, content.y, content.width, content.height}) {
        AppendDecimal(value, &bounds);
        bounds.push_back(',');
      }
      bounds.append(" orientation=");
      AppendDecimal(static_cast<int>(image_rotation), &bounds);
      Trace(bounds);
    }
    ABSL_RETURN_IF_ERROR(presenter.Draw(
        output_texture, viewport,
        {.blue_first = consumer.raw_bgrx(),
         .top_down = consumer.raw_bgrx(),
         .rotation = image_rotation,
         .source_left = static_cast<float>(content.x) / output_texture.width(),
         .source_top = static_cast<float>(content.y) / output_texture.height(),
         .source_right = static_cast<float>(content.x + content.width) /
                         output_texture.width(),
         .source_bottom = static_cast<float>(content.y + content.height) /
                          output_texture.height()}));
    if (first_frame) {
      symbian::api::system::DebugLog("camera GPU draw done");
    }
    DrawControls(&waiting_shapes, size, window->rotation(), viewfinder.mode());
    ABSL_RETURN_IF_ERROR(context.Swap());
    if (first_frame) {
      symbian::api::system::DebugLog("camera GPU swap done");
    }
    redraw_pending = false;
    if (poll_count < 8) {
      Trace("GPU: frame presented");
    }
    first_frame = false;
    ++sequence;
    ++presented_frames;
    if (live && presented_frames % 60 == 0) {
      const std::int64_t sample_time = time::MonotonicClock::NowNanoseconds();
      std::string rate("GPU: presented 60 frames in ");
      AppendDecimal(static_cast<int>(
                        (sample_time - presented_interval_start_ns) / 1000000),
                    &rate);
      rate.append(" ms");
      Trace(rate);
      presented_interval_start_ns = sample_time;
    }
    time::SleepFor(pacer.NextDelayNanoseconds());
  }
  Trace("GPU: normal exit");
  Trace("GPU: stream close begin");
  stream.Close();
  Trace("GPU: stream close returned");
  return absl::OkStatus();
}

}  // namespace camera_app
