// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "symbian/api/display/window_surface.h"

#include <cstdint>
#include <cstring>
#include <new>
#include <string_view>
#include <utility>

#include <absl/base/nullability.h>
#include <e32keys.h>
#include <fbs.h>
#include <w32std.h>

#include "symbian/native_status.h"
#include "window_task_identity.h"

namespace symbian::api::display {
namespace {

// AppArc's EApaSystemEventShutdown is 1. Keep this wire value at the
// Window Server boundary; apgtask.h is not part of the staged SDK headers.
constexpr std::int32_t kAppArcShutdownEvent = 1;

absl::Status NativeError(int error, const char* absl_nonnull context) {
  return symbian::StatusFromNativeError(error, context);
}

WindowKey KeyFromScanCode(int scan_code) {
  switch (scan_code) {
    case EStdKeyEscape:
      return WindowKey::kEscape;
    case EStdKeyBackspace:
      return WindowKey::kBackspace;
    case EStdKeyLeftArrow:
      return WindowKey::kLeft;
    case EStdKeyRightArrow:
      return WindowKey::kRight;
    case EStdKeyUpArrow:
      return WindowKey::kUp;
    case EStdKeyDownArrow:
      return WindowKey::kDown;
    case EStdKeyEnter:
      return WindowKey::kEnter;
    case EStdKeyDevice3:
      return WindowKey::kSelect;
    default:
      return WindowKey::kUnknown;
  }
}

struct DisplayOrientationInfo {
  std::uint32_t width;
  std::uint32_t height;
  std::int32_t stride;
  std::uint32_t reserved[5];
};

struct DisplayChannelInfo {
  std::uint32_t bits_per_pixel;
  std::uint32_t refresh_rate_hz;
  std::uint32_t available_rotations;
  std::int32_t pixel_format;
  DisplayOrientationInfo normal;
  DisplayOrientationInfo flipped;
  std::uint32_t composition_buffers;
  std::uint32_t reserved[5];
};

// Original dispchannel.h version 1.0 places the refresh rate at byte 4.
static_assert(sizeof(DisplayChannelInfo) == 104);

class DisplayRefreshChannel final : public RBusLogicalChannel {
 public:
  int Open() {
    _LIT(KDisplayDriverName, "displaychannel");
    return DoCreate(KDisplayDriverName, TVersion(1, 0, 1), 0, nullptr, nullptr);
  }

  int Read(TPckgBuf<DisplayChannelInfo>* absl_nonnull information) {
    return DoControl(2, information);
  }
};

}  // namespace

struct WindowSurface::Impl {
  RWsSession session;
  RWindowGroup* absl_nullable group = nullptr;
  RWindow* absl_nullable window = nullptr;
  CWsScreenDevice* absl_nullable screen = nullptr;
  CWindowGc* absl_nullable gc = nullptr;
  CFbsBitmap* absl_nullable bitmap = nullptr;
  RChunk frame_chunk;
  std::byte* absl_nullable frame_pixels = nullptr;
  std::size_t frame_bytes = 0;
  TRequestStatus event;
  TRequestStatus redraw;
  WindowSize size;
  WindowFrameMetrics metrics;
  bool session_open = false;
  bool fbs_open = false;
  bool group_open = false;
  bool window_open = false;
  bool requests_open = false;
  bool frame_chunk_open = false;
  bool measure_frames = false;

  void DestroyFrame() {
    frame_pixels = nullptr;
    frame_bytes = 0;
    if (frame_chunk_open) {
      frame_chunk.Close();
      frame_chunk_open = false;
    }
    delete bitmap;
    bitmap = nullptr;
  }

  void Close() {
    if (requests_open) {
      session.EventReadyCancel();
      session.RedrawReadyCancel();
      if (event == KRequestPending) {
        User::WaitForRequest(event);
      }
      if (redraw == KRequestPending) {
        User::WaitForRequest(redraw);
      }
      requests_open = false;
    }
    DestroyFrame();
    if (window_open) {
      window->Close();
    }
    if (group_open) {
      group->Close();
    }
    window_open = false;
    group_open = false;
    delete window;
    window = nullptr;
    delete group;
    group = nullptr;
    delete gc;
    gc = nullptr;
    delete screen;
    screen = nullptr;
    if (session_open) {
      session.Close();
    }
    if (fbs_open) {
      RFbsSession::Disconnect();
    }
    session_open = false;
    fbs_open = false;
    size = {};
  }

  absl::Status Present() {
    if (bitmap == nullptr || !window_open) {
      return absl::FailedPreconditionError("RGB565 frame is not ready");
    }
    const std::uint32_t before = measure_frames ? User::NTickCount() : 0;
    bitmap->LockHeap();
    void* absl_nullable destination = bitmap->DataAddress();
    if (destination == nullptr) {
      bitmap->UnlockHeap();
      return absl::InternalError("bitmap has no writable data");
    }
    std::memcpy(destination, frame_pixels, frame_bytes);
    bitmap->UnlockHeap();
    gc->Activate(*window);
    gc->BitBlt(TPoint(0, 0), bitmap);
    gc->Deactivate();
    session.Flush();
    if (measure_frames) {
      const std::uint32_t elapsed = User::NTickCount() - before;
      ++metrics.frames;
      metrics.total_native_ticks += elapsed;
      if (elapsed > metrics.max_native_ticks) {
        metrics.max_native_ticks = elapsed;
      }
    }
    return absl::OkStatus();
  }
};

WindowSurface::WindowSurface() : impl_(new (std::nothrow) Impl) {}

WindowSurface::WindowSurface(WindowSurface&& other) noexcept
    : impl_(std::exchange(other.impl_, nullptr)) {}

WindowSurface& WindowSurface::operator=(WindowSurface&& other) noexcept {
  if (this != &other) {
    Close();
    delete impl_;
    impl_ = std::exchange(other.impl_, nullptr);
  }
  return *this;
}

WindowSurface::~WindowSurface() {
  Close();
  delete impl_;
}

absl::StatusOr<WindowSize> WindowSurface::PrimarySize() {
  RWsSession session;
  const TInt connected = session.Connect();
  if (connected != KErrNone) {
    return NativeError(connected, "Window Server");
  }
  auto* absl_nullable screen = new CWsScreenDevice(session);
  if (screen == nullptr) {
    session.Close();
    return absl::ResourceExhaustedError("screen allocation failed");
  }
  const TInt constructed = screen->Construct();
  WindowSize size;
  if (constructed == KErrNone) {
    const TSize native = screen->SizeInPixels();
    size = {.width = native.iWidth, .height = native.iHeight};
  }
  delete screen;
  session.Close();
  if (constructed != KErrNone) {
    return NativeError(constructed, "screen");
  }
  if (size.width <= 0 || size.height <= 0) {
    return absl::FailedPreconditionError("invalid screen geometry");
  }
  return size;
}

std::optional<std::uint32_t> WindowSurface::PrimaryRefreshRateHz() {
  DisplayRefreshChannel channel;
  if (channel.Open() != KErrNone) {
    return std::nullopt;
  }
  TPckgBuf<DisplayChannelInfo> information;
  const int result = channel.Read(&information);
  channel.Close();
  if (result != KErrNone || information().refresh_rate_hz == 0 ||
      information().refresh_rate_hz > 1000) {
    return std::nullopt;
  }
  return information().refresh_rate_hz;
}

absl::Status WindowSurface::Open(std::string_view task_caption) {
  if (impl_ == nullptr) {
    return absl::ResourceExhaustedError("window owner allocation failed");
  }
  if (impl_->window_open) {
    return absl::FailedPreconditionError("window already open");
  }
  TInt result = RFbsSession::Connect();
  if (result != KErrNone) {
    return NativeError(result, "bitmap server");
  }
  impl_->fbs_open = true;
  result = impl_->session.Connect();
  if (result != KErrNone) {
    Close();
    return NativeError(result, "Window Server");
  }
  impl_->session_open = true;
  impl_->screen = new CWsScreenDevice(impl_->session);
  if (impl_->screen == nullptr) {
    return absl::ResourceExhaustedError("screen allocation failed");
  }
  result = impl_->screen->Construct();
  if (result != KErrNone) {
    return NativeError(result, "screen");
  }
  const TSize native = impl_->screen->SizeInPixels();
  if (native.iWidth <= 0 || native.iHeight <= 0 || native.iWidth > 2048 ||
      native.iHeight > 2048) {
    return absl::FailedPreconditionError("unsupported screen geometry");
  }
  impl_->size = {.width = native.iWidth, .height = native.iHeight};
  impl_->gc = new CWindowGc(impl_->screen);
  if (impl_->gc == nullptr) {
    return absl::ResourceExhaustedError("graphics allocation failed");
  }
  result = impl_->gc->Construct();
  if (result != KErrNone) {
    return NativeError(result, "graphics context");
  }
  // These owners must be constructed after RWsSession::Connect: their
  // constructors capture its live native handle.
  impl_->group = new (std::nothrow) RWindowGroup(impl_->session);
  if (impl_->group == nullptr) {
    return absl::ResourceExhaustedError("window group allocation failed");
  }
  result = impl_->group->Construct(1, ETrue);
  if (result != KErrNone) {
    return NativeError(result, "window group");
  }
  impl_->group_open = true;
  if (!task_caption.empty()) {
    const std::uint32_t uid = RProcess().SecureId().iId;
    result = internal::SetWindowTaskIdentity(impl_->group, uid, task_caption);
    if (result != KErrNone) {
      return NativeError(result, "window task identity");
    }
  }
  impl_->window = new (std::nothrow) RWindow(impl_->session);
  if (impl_->window == nullptr) {
    return absl::ResourceExhaustedError("window allocation failed");
  }
  result = impl_->window->Construct(*impl_->group, 2);
  if (result != KErrNone) {
    return NativeError(result, "window");
  }
  impl_->window_open = true;
  // Window Server filters drag and move events by default. Touch tracking
  // needs the drag stream while the finger remains down.
  impl_->window->PointerFilter(EPointerFilterDrag | EPointerFilterMove, 0);
  impl_->group->SetOrdinalPosition(0);
  impl_->window->SetExtent(TPoint(0, 0), native);
  impl_->window->SetVisible(ETrue);
  impl_->window->Activate();
  impl_->session.EventReady(&impl_->event);
  impl_->session.RedrawReady(&impl_->redraw);
  impl_->requests_open = true;
  impl_->session.Flush();
  return absl::OkStatus();
}

absl::StatusOr<Rgb565Frame> WindowSurface::CreateRgb565Frame() {
  if (impl_ == nullptr || !impl_->window_open) {
    return absl::FailedPreconditionError("window is closed");
  }
  if (impl_->bitmap != nullptr) {
    return absl::FailedPreconditionError("frame already exists");
  }
  impl_->bitmap = new CFbsBitmap;
  if (impl_->bitmap == nullptr) {
    return absl::ResourceExhaustedError("bitmap allocation failed");
  }
  const TInt result = impl_->bitmap->Create(
      TSize(impl_->size.width, impl_->size.height), EColor64K);
  if (result != KErrNone) {
    return NativeError(result, "RGB565 bitmap");
  }
  const int pitch = CFbsBitmap::ScanLineLength(impl_->size.width, EColor64K);
  if (pitch <= 0) {
    return absl::InternalError("bitmap has invalid stride");
  }
  impl_->frame_bytes = static_cast<std::size_t>(pitch) * impl_->size.height;
  const TInt allocated =
      impl_->frame_chunk.CreateLocal(static_cast<TInt>(impl_->frame_bytes),
                                     static_cast<TInt>(impl_->frame_bytes));
  if (allocated != KErrNone) {
    impl_->frame_bytes = 0;
    return NativeError(allocated, "frame allocation");
  }
  impl_->frame_chunk_open = true;
  impl_->frame_pixels = reinterpret_cast<std::byte*>(impl_->frame_chunk.Base());
  return Rgb565Frame{
      .pixels = std::span<std::byte>(impl_->frame_pixels, impl_->frame_bytes),
      .pitch_bytes = pitch,
      .size = impl_->size,
  };
}

absl::Status WindowSurface::Present() {
  if (impl_ == nullptr) {
    return absl::FailedPreconditionError("window owner unavailable");
  }
  return impl_->Present();
}

absl::StatusOr<std::optional<WindowInput>> WindowSurface::PollInput() {
  if (impl_ == nullptr || !impl_->requests_open) {
    return absl::FailedPreconditionError("window is closed");
  }
  std::optional<WindowInput> input;
  if (impl_->event != KRequestPending) {
    const TInt result = impl_->event.Int();
    if (result != KErrNone) {
      return NativeError(result, "window event");
    }
    TWsEvent event;
    impl_->session.GetEvent(event);
    if (event.Handle() == 2 && event.Type() == EEventPointer) {
      const TPointerEvent* absl_nonnull pointer = event.Pointer();
      WindowInputKind kind = WindowInputKind::kPointerMove;
      if (pointer->iType == TPointerEvent::EButton1Down) {
        kind = WindowInputKind::kPointerDown;
      } else if (pointer->iType == TPointerEvent::EButton1Up) {
        kind = WindowInputKind::kPointerUp;
      }
      input = WindowInput{
          .kind = kind, .x = pointer->iPosition.iX, .y = pointer->iPosition.iY};
    } else if (event.Type() == EEventKeyDown || event.Type() == EEventKeyUp) {
      input = WindowInput{
          .kind = event.Type() == EEventKeyDown ? WindowInputKind::kKeyDown
                                                : WindowInputKind::kKeyUp,
          .key = KeyFromScanCode(static_cast<int>(event.Key()->iScanCode)),
      };
    } else if (event.Type() == EEventFocusGained ||
               event.Type() == EEventFocusLost) {
      input = WindowInput{
          .kind = event.Type() == EEventFocusGained
                      ? WindowInputKind::kFocusGained
                      : WindowInputKind::kFocusLost,
      };
    } else if (event.Type() == EEventUser &&
               *reinterpret_cast<const TInt*>(event.EventData()) ==
                   kAppArcShutdownEvent) {
      input = WindowInput{.kind = WindowInputKind::kCloseRequested};
    } else if (event.Type() == EEventScreenDeviceChanged) {
      const TSize native = impl_->screen->SizeInPixels();
      if (native.iWidth > 0 && native.iHeight > 0) {
        impl_->window->SetExtent(TPoint(0, 0), native);
        impl_->size = {.width = native.iWidth, .height = native.iHeight};
      }
      input = WindowInput{.kind = WindowInputKind::kDisplayChanged};
    }
    impl_->session.EventReady(&impl_->event);
  }
  if (impl_->redraw != KRequestPending) {
    const TInt result = impl_->redraw.Int();
    if (result != KErrNone) {
      return NativeError(result, "window redraw");
    }
    TWsRedrawEvent redraw;
    impl_->session.GetRedraw(redraw);
    if (redraw.Handle() == 2) {
      impl_->window->BeginRedraw(redraw.Rect());
      if (impl_->bitmap != nullptr) {
        impl_->Present().IgnoreError();
      }
      impl_->window->EndRedraw();
    }
    impl_->session.RedrawReady(&impl_->redraw);
  }
  impl_->session.Flush();
  return input;
}

void WindowSurface::DestroyFrame() {
  if (impl_ != nullptr) {
    impl_->DestroyFrame();
  }
}

void WindowSurface::Close() {
  if (impl_ != nullptr) {
    impl_->Close();
  }
}

WindowSize WindowSurface::size() const {
  return impl_ == nullptr ? WindowSize{} : impl_->size;
}

void* absl_nullable WindowSurface::NativeWindowHandle() const {
  if (impl_ == nullptr || !impl_->window_open) {
    return nullptr;
  }
  return impl_->window;
}

void WindowSurface::set_measure_frames(bool enabled) {
  if (impl_ != nullptr) {
    impl_->measure_frames = enabled;
  }
}

WindowFrameMetrics WindowSurface::frame_metrics() const {
  return impl_ == nullptr ? WindowFrameMetrics{} : impl_->metrics;
}

}  // namespace symbian::api::display
