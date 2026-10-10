// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "symbian/api/display/screen_control.h"

#include <cstring>
#include <memory>

#include <absl/base/nullability.h>
#include <e32event.h>
#include <fbs.h>
#include <w32std.h>

#include "symbian/native_status.h"

namespace symbian::api::display {

absl::StatusOr<ScreenCapture> CapturePrimaryScreen() {
  RWsSession session;
  TInt result = session.Connect();
  if (result != KErrNone) {
    return symbian::StatusFromNativeError(result, "screen session");
  }
  std::unique_ptr<CWsScreenDevice> screen(new CWsScreenDevice(session));
  if (screen == nullptr) {
    session.Close();
    return absl::ResourceExhaustedError("screen device allocation failed");
  }
  result = screen->Construct();
  if (result != KErrNone) {
    screen.reset();
    session.Close();
    return symbian::StatusFromNativeError(result, "screen device");
  }
  const TSize size = screen->SizeInPixels();
  if (size.iWidth <= 0 || size.iHeight <= 0 || size.iWidth > 4096 ||
      size.iHeight > 4096) {
    screen.reset();
    session.Close();
    return absl::FailedPreconditionError("invalid screen size");
  }
  result = RFbsSession::Connect();
  if (result != KErrNone) {
    screen.reset();
    session.Close();
    return symbian::StatusFromNativeError(result, "bitmap session");
  }
  std::unique_ptr<CFbsBitmap> bitmap(new CFbsBitmap());
  if (bitmap != nullptr) {
    result = bitmap->Create(size, EColor64K);
    if (result == KErrNone) {
      result = screen->CopyScreenToBitmap(bitmap.get());
    }
  } else {
    result = KErrNoMemory;
  }
  ScreenCapture capture;
  if (result == KErrNone) {
    capture.width = size.iWidth;
    capture.height = size.iHeight;
    capture.stride_bytes = CFbsBitmap::ScanLineLength(size.iWidth, EColor64K);
    const std::size_t byte_count =
        static_cast<std::size_t>(capture.stride_bytes) * size.iHeight;
    capture.pixels.resize(byte_count);
    bitmap->LockHeap();
    const void* absl_nullable source = bitmap->DataAddress();
    if (source != nullptr) {
      std::memcpy(capture.pixels.data(), source, byte_count);
    } else {
      result = KErrGeneral;
    }
    bitmap->UnlockHeap();
  }
  bitmap.reset();
  RFbsSession::Disconnect();
  screen.reset();
  session.Close();
  if (result != KErrNone) {
    return symbian::StatusFromNativeError(result, "screen capture");
  }
  return capture;
}

absl::Status SendPrimaryPointerEvent(PointerAction action, int x, int y) {
  RWsSession session;
  TInt result = session.Connect();
  if (result != KErrNone) {
    return symbian::StatusFromNativeError(result, "pointer session");
  }
  CWsScreenDevice screen(session);
  result = screen.Construct();
  if (result != KErrNone) {
    session.Close();
    return symbian::StatusFromNativeError(result, "pointer screen");
  }
  const TSize size = screen.SizeInPixels();
  if (x < 0 || y < 0 || x >= size.iWidth || y >= size.iHeight) {
    session.Close();
    return absl::InvalidArgumentError("pointer outside primary screen");
  }
  TRawEvent event;
  event.Set(action == PointerAction::kDown
                ? TRawEvent::EButton1Down
                : action == PointerAction::kUp ? TRawEvent::EButton1Up
                                               : TRawEvent::EPointerMove,
            x, y);
  session.SimulateRawEvent(event);
  session.Flush();
  session.Close();
  return absl::OkStatus();
}

}  // namespace symbian::api::display
