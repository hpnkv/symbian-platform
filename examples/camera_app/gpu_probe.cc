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

#include "capture_preferences.h"
#include "symbian/api/camera/camera.h"
#include "symbian/api/camera/camera_stream.h"
#include "symbian/api/camera/frame.h"
#include "symbian/api/camera/gles2_frame_backend.h"
#include "symbian/api/display/gles2_texture.h"
#include "symbian/api/display/gles_window_context.h"
#include "symbian/api/display/window_surface.h"
#include "symbian/api/storage/storage.h"
#include "symbian/api/system/debug_log.h"
#include "symbian/api/time/frame_pacer.h"
#include "symbian/api/time/monotonic_clock.h"
#include "symbian/api/time/sleep.h"
#include "trace.h"

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
      if (absl::Status resized =
              texture_->Resize(frame.layout.width, frame.layout.height);
          !resized.ok()) {
        return resized;
      }
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
    absl::Status uploaded = camera::TransformFrame(
        frame, output, camera::ResampleFilter::kNearest, backend_);
    if (uploaded.ok()) {
      raw_bgrx_ = bgrx;
      has_frame_ = true;
    }
    return uploaded;
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

int RunGpuWindow(display::WindowSurface* absl_nonnull window) {
  Trace("GPU: discover begin");
  const auto inventory = camera::DiscoverCameras();
  Trace(inventory.ok() ? "GPU: discover done" : "GPU: discover error");
  const bool discovered = inventory.ok();
  const bool present = inventory.ok() && inventory->available_count > 0;
  camera::CameraStream stream;
  const display::DisplayRotation opening_rotation = window->rotation();
  const auto preferences =
      CapturePreferences(window->size().height >= window->size().width);
  if (present) {
    Trace("GPU: stream open begin");
  }
  if (absl::Status opened =
          present ? stream.Open(0, preferences) : absl::OkStatus();
      !opened.ok()) {
    RecordFailure(opened);
    return 1;
  }
  bool live = present;
  Trace(live ? "GPU: stream open done" : "GPU: stream unavailable");
  symbian::api::system::DebugLog(live ? "camera GPU capture opening"
                                      : "camera GPU capture unavailable");
  std::int64_t opening_time = time::MonotonicClock::NowNanoseconds();
  bool received_frame = false;
  bool capturing = live;

  display::GlesWindowContext context;
  if (absl::Status context_opened = context.Open(
          window, 2,
          {.red_bits = 8, .green_bits = 8, .blue_bits = 8, .depth_bits = 0});
      !context_opened.ok()) {
    Trace("GPU: context open error");
    RecordFailure(context_opened);
    return 1;
  }
  Trace("GPU: context open done");
  display::Gles2Texture source_texture;
  display::Gles2Texture output_texture;
  display::Gles2TexturePresenter presenter;
  display::WindowSize window_size = window->size();
  auto surface_size = context.SurfaceSize();
  if (!surface_size.ok()) {
    RecordFailure(surface_size.status());
    return 1;
  }
  display::WindowSize size = *surface_size;
  TraceGeometry(window_size, size, window->rotation());
  if (!source_texture.Resize(2, 2).ok() || !output_texture.Resize(2, 2).ok() ||
      !presenter.Open().ok()) {
    Trace("GPU: texture or presenter error");
    return 1;
  }
  Trace("GPU: texture and presenter done");
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
  std::uint64_t sequence = 0;
  std::uint64_t poll_count = 0;
  std::uint64_t presented_frames = 0;
  std::int64_t presented_interval_start_ns =
      time::MonotonicClock::NowNanoseconds();
  while (running) {
    ++poll_count;
    for (int count = 0; count < 64; ++count) {
      auto input = window->PollInput();
      if (!input.ok()) {
        Trace("GPU: input error");
        RecordFailure(input.status());
        return 1;
      }
      if (!input->has_value()) {
        break;
      }
      switch ((**input).kind) {
        case display::WindowInputKind::kPointerDown:
          ToggleFilter(&filter);
          break;
        case display::WindowInputKind::kKeyDown:
          if ((**input).key == display::WindowKey::kEscape) {
            Trace("GPU: escape key");
            running = false;
          } else if ((**input).key == display::WindowKey::kSelect ||
                     (**input).key == display::WindowKey::kEnter) {
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
      if (absl::Status refreshed = context.RefreshSurface(); !refreshed.ok()) {
        RecordFailure(refreshed);
        return 1;
      }
      Trace("GPU: surface refresh done");
      auto refreshed_size = context.SurfaceSize();
      if (!refreshed_size.ok()) {
        RecordFailure(refreshed_size.status());
        return 1;
      }
      size = *refreshed_size;
      TraceGeometry(window_size, size, window->rotation());
      surface_refresh_pending = false;
      redraw_pending = true;
    }
    if (live && !capturing) {
      Trace("GPU: capture resume begin");
      if (absl::Status resumed = stream.Open(0, preferences); !resumed.ok()) {
        RecordFailure(resumed);
        return 1;
      }
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
      auto polled = stream.PollScoped(consumer.Borrow());
      if (poll_count < 8) {
        Trace(polled.ok() ? "GPU: poll done" : "GPU: poll error");
      }
      if (!polled.ok()) {
        RecordFailure(polled.status());
        return 1;
      } else if (*polled) {
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
      RecordFailure(
          absl::DeadlineExceededError("camera GPU capture timed out"));
      return 1;
    }
    if (live && !received_frame) {
      if (poll_count < 8) {
        Trace("GPU: wait frame present begin");
      }
      if (!presenter
               .Clear(size.width, size.height, TraceReady() ? 0.0f : 1.0f, 0.0f,
                      1.0f)
               .ok() ||
          !context.Swap().ok()) {
        Trace("GPU: wait frame present error");
        return 1;
      }
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

    absl::Status transform_status = absl::OkStatus();
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
      transform_status = camera::TransformFrame(
          memory, upload, camera::ResampleFilter::kNearest, &backend_view);
      if (first_frame) {
        symbian::api::system::DebugLog(
            transform_status.ok() ? "camera GPU synthetic upload done"
                                  : "camera GPU synthetic upload failed");
      }
      if (transform_status.ok()) {
        transform_status =
            camera::TransformFrame(source, output, filter, &backend_view);
      }
    }

    const bool transformed = transform_status.ok();

    if (first_frame) {
      symbian::api::system::DebugLog(transformed
                                         ? "camera GPU transform done"
                                         : "camera GPU transform failed");
    }

    const float red = !live && (!TraceReady() || !discovered) ? 1.0f : 0.0f;
    const float green = !live && discovered ? 1.0f : 0.0f;
    if (!transformed) {
      Trace("GPU: transform error");
      RecordFailure(transform_status);
      return 1;
    }
    const bool cleared =
        presenter.Clear(size.width, size.height, red, green, 0.0f).ok();
    if (first_frame) {
      symbian::api::system::DebugLog(cleared ? "camera GPU clear done"
                                             : "camera GPU clear failed");
    }
    if (!cleared) {
      Trace("GPU: clear error");
      return 1;
    }
    const camera::FrameRotation camera_rotation =
        PresentationRotation(window->rotation(), opening_rotation);
    const display::TextureRotation image_rotation =
        ToTextureRotation(camera_rotation);
    const bool quarter_turn =
        image_rotation == display::TextureRotation::kClockwise90 ||
        image_rotation == display::TextureRotation::kClockwise270;
    const camera::FrameRect content =
        live ? consumer.content()
             : camera::FrameRect{0, 0, output_texture.width(),
                                 output_texture.height()};
    const int image_width = quarter_turn ? content.height : content.width;
    const int image_height = quarter_turn ? content.width : content.height;
    auto viewport = display::AspectFitViewport(size.width, size.height,
                                               image_width, image_height);
    if (!viewport.ok()) {
      RecordFailure(viewport.status());
      return 1;
    }
    const bool drawn =
        presenter
            .Draw(
                output_texture, *viewport,
                {.blue_first = consumer.raw_bgrx(),
                 .top_down = consumer.raw_bgrx(),
                 .rotation = image_rotation,
                 .source_left =
                     static_cast<float>(content.x) / output_texture.width(),
                 .source_top =
                     static_cast<float>(content.y) / output_texture.height(),
                 .source_right = static_cast<float>(content.x + content.width) /
                                 output_texture.width(),
                 .source_bottom =
                     static_cast<float>(content.y + content.height) /
                     output_texture.height()})
            .ok();
    if (first_frame) {
      symbian::api::system::DebugLog(drawn ? "camera GPU draw done"
                                           : "camera GPU draw failed");
    }
    if (!drawn) {
      Trace("GPU: draw error");
      return 1;
    }
    const bool swapped = context.Swap().ok();
    if (first_frame) {
      symbian::api::system::DebugLog(swapped ? "camera GPU swap done"
                                             : "camera GPU swap failed");
    }
    if (!swapped) {
      Trace("GPU: swap error");
      return 1;
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
  return 0;
}

}  // namespace camera_app
