// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "native_camera_stream.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <new>
#include <optional>
#include <string_view>
#include <vector>

#include <absl/base/nullability.h>

#include "symbian/api/camera/frame.h"
#include "symbian/api/time/monotonic_clock.h"

namespace std {
bool uncaught_exception();
}

#define __EXCEPTION__
#include <e32base.h>
#undef __EXCEPTION__
#include <ECam.h>
#include <fbs.h>
#if defined(SYMBIAN_CAMERA_NATIVE_TRACE)
#include <f32file.h>
#endif

namespace {

CCamera::TFormat EcamFormat(int format) {
  switch (format) {
    case static_cast<int>(symbian::api::camera::PixelFormat::kGray8):
      return CCamera::EFormatMonochrome;
    case static_cast<int>(symbian::api::camera::PixelFormat::kBgr565):
      return CCamera::EFormat16BitRGB565;
    case static_cast<int>(symbian::api::camera::PixelFormat::kRgbx8888):
      return CCamera::EFormat32BitRGB888;
    case static_cast<int>(symbian::api::camera::PixelFormat::kYuv420Planar):
      return CCamera::EFormatYUV420Planar;
    default:
      return CCamera::EFormatMonochrome;
  }
}

class CameraState final : public MCameraObserver2, public MCameraObserver {
 public:
  CameraState(const NativeCameraRequest* absl_nonnull requests, int count)
      : size_(requests[0].width, requests[0].height),
        format_(requests[0].format),
        request_count_(count) {
    for (int i = 0; i < count; ++i) {
      requests_[i] = requests[i];
    }
  }

  TInt Open(int index) {
    index_ = index;
    OpenNativeTrace();
    NativeTrace("open: cleanup begin");
    cleanup_ = CTrapCleanup::New();
    if (cleanup_ == nullptr) {
      return KErrNoMemory;
    }
    previous_scheduler_ = CActiveScheduler::Current();
    if (previous_scheduler_ == nullptr) {
      CActiveScheduler::Install(&scheduler_);
      installed_scheduler_ = true;
    }
    NativeTrace("open: New2L begin");
    TRAPD(error, camera_ = CCamera::New2L(*this, index, 0));
    if (error != KErrNone) {
      NativeTrace("open: New2L error");
      return error;
    }
    TCameraInfo info;
    camera_->CameraInfo(info);
    bitmap_supported_ = (info.iOptionsSupported &
                         TCameraInfo::EViewFinderBitmapsSupported) != 0;
    NativeTrace(bitmap_supported_ ? "open: bitmap viewfinder supported"
                                  : "open: bitmap viewfinder unavailable");
    NativeTrace(
        (info.iOptionsSupported & TCameraInfo::EViewFinderDirectSupported) != 0
            ? "open: direct viewfinder supported"
            : "open: direct viewfinder unavailable");
    NativeTrace("open: Reserve begin");
    camera_->Reserve();
    NativeTrace("open: Reserve returned");
    return KErrNone;
  }

  TInt Poll(NativeCameraFrame* absl_nonnull frame) {
    ++poll_count_;
    if (held_ != nullptr || bitmap_held_slot_ >= 0) {
      return KErrNone;
    }
    if (reserve_retry_at_ns_ != 0 &&
        symbian::api::time::MonotonicClock::NowNanoseconds() >=
            reserve_retry_at_ns_) {
      reserve_retry_at_ns_ = 0;
      ++reserve_retry_count_;
      NativeTrace("reserve: retry begin");
      camera_->Reserve();
      NativeTrace("reserve: retry returned");
    }
    for (int count = 0; count < 8; ++count) {
      TInt callback_error = KErrNone;
      if (poll_count_ < 12) {
        NativeTrace("poll: dispatch begin");
      }
      if (!CActiveScheduler::RunIfReady(callback_error,
                                        CActive::EPriorityIdle)) {
        if (poll_count_ < 12) {
          NativeTrace("poll: no ready object");
        }
        break;
      }
      if (poll_count_ < 12) {
        NativeTrace("poll: dispatch returned");
      }
      if (callback_error != KErrNone) {
        error_ = callback_error;
        break;
      }
      if (scoped_delivered_) {
        break;
      }
    }
    if (error_ != KErrNone) {
      return error_;
    }
    if (scoped_delivered_) {
      return KErrNone;
    }
    if (start_pending_) {
      start_pending_ = false;
      NativeTrace("poll: deferred viewfinder start");
      error_ = StartViewFinder();
      if (error_ != KErrNone) {
        return error_;
      }
    }
    if (legacy_mode_) {
      if (bitmap_pending_slot_ < 0) {
        return KErrNone;
      }
      const int slot = bitmap_pending_slot_;
      bitmap_pending_slot_ = -1;
      bitmap_held_slot_ = slot;
      const BitmapFrame& bitmap = bitmap_frames_[slot];
      *frame = {.data = bitmap.pixels.data(),
                .bytes = static_cast<int>(bitmap.pixels.size()),
                .width = bitmap.width,
                .height = bitmap.height,
                .stride = bitmap.stride,
                .format = bitmap.format,
                .mapped = false,
                .sequence = bitmap.sequence,
                .capture_time_ns = bitmap.capture_time_ns,
                .release_owner = this};
      return KErrNone;
    }
    if (pending_ == nullptr) {
      return KErrNone;
    }
    NativeTrace("poll: pending frame");
    held_ = pending_;
    pending_ = nullptr;
    const TInt frame_bytes = held_->FrameSize(0);
    NativeTrace("poll: frame size returned");
    if (frame_bytes <= 0) {
      return FailFrame(KErrCorrupt);
    }
    const unsigned char* absl_nullable pixels = nullptr;
    bool mapped = false;
    TRAPD(chunk_error, pixels = ChunkPixelsL());
    NativeTrace("poll: chunk access returned");
    if (chunk_error == KErrNone) {
      mapped = true;
    } else {
      TRAPD(data_error, pixels = DataPixelsL());
      NativeTrace("poll: descriptor access returned");
      if (data_error != KErrNone) {
        return FailFrame(data_error);
      }
    }
    const bool yuv =
        format_ ==
        static_cast<int>(symbian::api::camera::PixelFormat::kYuv420Planar);
    const int pixel_bytes =
        format_ == static_cast<int>(symbian::api::camera::PixelFormat::kBgr565)
            ? 2
            : (format_ == static_cast<int>(
                              symbian::api::camera::PixelFormat::kRgbx8888)
                   ? 4
                   : 1);
    const std::int64_t row_bytes =
        static_cast<std::int64_t>(size_.iWidth) * pixel_bytes;
    const std::int64_t minimum = row_bytes * size_.iHeight * (yuv ? 3 : 2) / 2;
    if (size_.iWidth <= 0 || size_.iHeight <= 0 || size_.iWidth > 8192 ||
        size_.iHeight > 8192 || pixels == nullptr || frame_bytes < minimum ||
        (yuv ? frame_bytes != minimum : frame_bytes % size_.iHeight != 0)) {
      return FailFrame(KErrCorrupt);
    }
    const int stride = yuv ? size_.iWidth : frame_bytes / size_.iHeight;
    *frame = {.data = pixels,
              .bytes = frame_bytes,
              .width = size_.iWidth,
              .height = size_.iHeight,
              .stride = stride,
              .format = format_,
              .mapped = mapped,
              .sequence = sequence_,
              .capture_time_ns = capture_time_ns_,
              .release_owner = this};
    NativeTrace("poll: frame returned");
    return KErrNone;
  }

  TInt PollScoped(NativeCameraFrameConsumer consumer,
                  void* absl_nonnull context, bool* absl_nonnull delivered) {
    if (scoped_consumer_.has_value()) {
      return KErrInUse;
    }
    scoped_consumer_ = consumer;
    scoped_context_ = context;
    scoped_delivered_ = false;
    NativeCameraFrame frame;
    const TInt error = Poll(&frame);
    scoped_consumer_.reset();
    scoped_context_ = nullptr;
    if (error != KErrNone) {
      return error;
    }
    if (frame.data != nullptr) {
      consumer(&frame, context);
      ReleaseHeld();
      *delivered = true;
    } else {
      *delivered = scoped_delivered_;
    }
    return KErrNone;
  }

  void Stop() {
    if (closed_) {
      return;
    }
    closed_ = true;
    if (camera_ != nullptr && viewfinder_started_) {
      NativeTrace("stop: StopViewFinder begin");
      camera_->StopViewFinder();
      NativeTrace("stop: StopViewFinder returned");
      viewfinder_started_ = false;
    }
    if (pending_ != nullptr) {
      pending_->Release();
      pending_ = nullptr;
    }
    bitmap_pending_slot_ = -1;
    // Bitmap frames live in SDK-owned slots, so their camera can close even
    // while a caller still holds a frame lease.
    if (bitmap_held_slot_ >= 0 && held_ == nullptr) {
      Finalize();
      return;
    }
    // Native ECam buffers still need their camera until the lease is released.
    if (held_ != nullptr) {
      ReleaseThreadInfrastructure();
    }
    if (held_ == nullptr && bitmap_held_slot_ < 0) {
      Finalize();
      delete this;
    }
  }

  void ReleaseHeld() {
    if (held_ != nullptr) {
      held_->Release();
      held_ = nullptr;
    }
    bitmap_held_slot_ = -1;
    if (closed_) {
      Finalize();
      delete this;
    }
  }

  void Finalize() {
    NativeTrace("finalize: begin");
    if (held_ != nullptr) {
      held_->Release();
      held_ = nullptr;
    }
    if (pending_ != nullptr) {
      pending_->Release();
      pending_ = nullptr;
    }
    bitmap_pending_slot_ = -1;
    if (camera_ != nullptr) {
      if (viewfinder_started_) {
        NativeTrace("finalize: StopViewFinder begin");
        camera_->StopViewFinder();
        NativeTrace("finalize: StopViewFinder returned");
      }
      if (powered_) {
        NativeTrace("finalize: PowerOff begin");
        camera_->PowerOff();
        NativeTrace("finalize: PowerOff returned");
      }
      if (reserved_) {
        NativeTrace("finalize: Release begin");
        camera_->Release();
        NativeTrace("finalize: Release returned");
      }
      NativeTrace("finalize: camera delete begin");
      delete camera_;
      NativeTrace("finalize: camera delete returned");
      camera_ = nullptr;
    }
    NativeTrace("finalize: thread cleanup begin");
    ReleaseThreadInfrastructure();
    NativeTrace("finalize: thread cleanup returned");
    if (fbs_connected_) {
      NativeTrace("finalize: FBS disconnect begin");
      RFbsSession::Disconnect();
      NativeTrace("finalize: FBS disconnect returned");
      fbs_connected_ = false;
    }
    NativeTrace("finalize: complete");
    CloseNativeTrace();
  }

  void HandleEvent(const TECAMEvent& event) override {
    NativeTrace("callback: event");
    if (closed_) {
      return;
    }
    if (event.iEventType == KUidECamEventReserveComplete) {
      NativeTrace("callback: reserve complete");
      if (event.iErrorCode == KErrNone) {
        reserved_ = true;
        NativeTrace("callback: PowerOn begin");
        camera_->PowerOn();
        NativeTrace("callback: PowerOn returned");
      } else {
        ReserveFailed(event.iErrorCode);
      }
    } else if (event.iEventType == KUidECamEventPowerOnComplete) {
      NativeTrace("callback: power complete");
      if (event.iErrorCode == KErrNone) {
        powered_ = true;
        TraceDigitalZoom();
        start_pending_ = true;
      } else {
        error_ = event.iErrorCode;
      }
    } else if (event.iEventType == KUidECamEventCameraNoLongerReserved) {
      error_ = KErrAccessDenied;
    }
  }

  void ViewFinderReady(MCameraBuffer& buffer, TInt error) override {
    NativeTrace("callback: viewfinder ready");
    // ECam documents a null buffer on failure despite the reference in the
    // observer signature. Do not touch that reference until success is known.
    if (error != KErrNone) {
      error_ = error;
      return;
    }
    if (closed_ || pending_ != nullptr) {
      buffer.Release();
      return;
    }
    if (buffer.NumFrames() <= 0) {
      buffer.Release();
      error_ = KErrCorrupt;
      return;
    }
    pending_ = &buffer;
    NativeTrace("callback: frame pending");
    ++sequence_;
    capture_time_ns_ = symbian::api::time::MonotonicClock::NowNanoseconds();
  }

  void ImageBufferReady(MCameraBuffer& buffer, TInt error) override {
    if (error == KErrNone) {
      buffer.Release();
    }
  }

  void VideoBufferReady(MCameraBuffer& buffer, TInt error) override {
    if (error == KErrNone) {
      buffer.Release();
    }
  }

  void ReserveComplete(TInt error) override {
    NativeTrace("callback: bitmap reserve complete");
    if (closed_) {
      return;
    }
    if (error != KErrNone) {
      ReserveFailed(error);
      return;
    }
    reserved_ = true;
    camera_->PowerOn();
  }

  void PowerOnComplete(TInt error) override {
    NativeTrace("callback: bitmap power complete");
    if (closed_) {
      return;
    }
    if (error != KErrNone) {
      error_ = error;
      return;
    }
    powered_ = true;
    TraceDigitalZoom();
    start_pending_ = true;
  }

  void ViewFinderFrameReady(CFbsBitmap& bitmap) override {
    if (closed_) {
      return;
    }
    const TSize size = bitmap.SizeInPixels();
    if (bitmap_callback_count_ < 5 || bitmap_callback_count_ % 30 == 0) {
      TBuf8<96> detail;
      detail.Format(_L8("bitmap: frame %d size %dx%d mode %d elapsed_ms %d"),
                    bitmap_callback_count_ + 1, size.iWidth, size.iHeight,
                    static_cast<int>(bitmap.DisplayMode()),
                    static_cast<int>(
                        (symbian::api::time::MonotonicClock::NowNanoseconds() -
                         bitmap_started_ns_) /
                        1000000));
      NativeTrace(std::string_view(reinterpret_cast<const char*>(detail.Ptr()),
                                   detail.Length()));
    }
    ++bitmap_callback_count_;
    if (size.iWidth <= 0 || size.iHeight <= 0 || size.iWidth > 8192 ||
        size.iHeight > 8192) {
      error_ = KErrCorrupt;
      return;
    }
    const TDisplayMode mode = bitmap.DisplayMode();
    if (scoped_consumer_.has_value() &&
        (mode == EColor16MU || mode == EColor64K)) {
      const TInt stride = CFbsBitmap::ScanLineLength(size.iWidth, mode);
      const TInt pixel_bytes = mode == EColor16MU ? 4 : 2;
      if (stride < size.iWidth * pixel_bytes ||
          static_cast<std::int64_t>(stride) * size.iHeight > 64 * 1024 * 1024) {
        error_ = KErrCorrupt;
        return;
      }
      bitmap.LockHeap();
      const TUint32* absl_nullable address = bitmap.DataAddress();
      if (address != nullptr) {
        NativeCameraFrame borrowed{
            .data = reinterpret_cast<const unsigned char*>(address),
            .bytes = stride * size.iHeight,
            .width = size.iWidth,
            .height = size.iHeight,
            .stride = stride,
            .format = static_cast<int>(
                mode == EColor16MU
                    ? symbian::api::camera::PixelFormat::kBgrx8888
                    : symbian::api::camera::PixelFormat::kRgb565),
            .mapped = true,
            .sequence = ++sequence_,
            .capture_time_ns =
                symbian::api::time::MonotonicClock::NowNanoseconds(),
        };
        (*scoped_consumer_)(&borrowed, scoped_context_);
        scoped_delivered_ = true;
      }
      bitmap.UnlockHeap();
      if (address == nullptr) {
        error_ = KErrCorrupt;
      }
      return;
    }
    int slot = -1;
    for (int candidate = 0; candidate < 2; ++candidate) {
      if (candidate != bitmap_pending_slot_ && candidate != bitmap_held_slot_) {
        slot = candidate;
        break;
      }
    }
    if (slot < 0) {
      return;
    }
    BitmapFrame* absl_nonnull result = &bitmap_frames_[slot];
    result->width = size.iWidth;
    result->height = size.iHeight;
    if (bitmap.DisplayMode() == EColor64K) {
      const TInt stride = CFbsBitmap::ScanLineLength(size.iWidth, EColor64K);
      if (stride < size.iWidth * 2 ||
          static_cast<std::int64_t>(stride) * size.iHeight > 64 * 1024 * 1024) {
        error_ = KErrCorrupt;
        return;
      }
      result->pixels.resize(static_cast<std::size_t>(stride) * size.iHeight);
      bitmap.LockHeap();
      const TUint32* absl_nullable source = bitmap.DataAddress();
      if (source != nullptr) {
        std::memcpy(result->pixels.data(), source, result->pixels.size());
      }
      bitmap.UnlockHeap();
      if (source == nullptr) {
        error_ = KErrCorrupt;
        return;
      }
      result->stride = stride;
      result->format =
          static_cast<int>(symbian::api::camera::PixelFormat::kRgb565);
    } else if (bitmap.DisplayMode() == EColor16MU) {
      const TInt stride = CFbsBitmap::ScanLineLength(size.iWidth, EColor16MU);
      if (stride < size.iWidth * 4 ||
          static_cast<std::int64_t>(stride) * size.iHeight > 64 * 1024 * 1024) {
        error_ = KErrCorrupt;
        return;
      }
      result->pixels.resize(static_cast<std::size_t>(stride) * size.iHeight);
      bitmap.LockHeap();
      const TUint32* absl_nullable data = bitmap.DataAddress();
      if (data != nullptr) {
        std::memcpy(result->pixels.data(), data, result->pixels.size());
      }
      bitmap.UnlockHeap();
      if (data == nullptr) {
        error_ = KErrCorrupt;
        return;
      }
      result->stride = stride;
      result->format =
          static_cast<int>(symbian::api::camera::PixelFormat::kBgrx8888);
    } else {
      const std::int64_t bytes =
          static_cast<std::int64_t>(size.iWidth) * size.iHeight * 4;
      if (bytes > 64 * 1024 * 1024) {
        error_ = KErrCorrupt;
        return;
      }
      result->pixels.resize(static_cast<std::size_t>(bytes));
      for (TInt y = 0; y < size.iHeight; ++y) {
        for (TInt x = 0; x < size.iWidth; ++x) {
          TRgb color;
          bitmap.GetPixel(color, TPoint(x, y));
          const std::size_t offset =
              (static_cast<std::size_t>(y) * size.iWidth + x) * 4;
          result->pixels[offset] = static_cast<unsigned char>(color.Red());
          result->pixels[offset + 1] =
              static_cast<unsigned char>(color.Green());
          result->pixels[offset + 2] = static_cast<unsigned char>(color.Blue());
          result->pixels[offset + 3] = 255;
        }
      }
      result->stride = size.iWidth * 4;
      result->format =
          static_cast<int>(symbian::api::camera::PixelFormat::kRgbx8888);
    }
    result->sequence = ++sequence_;
    result->capture_time_ns =
        symbian::api::time::MonotonicClock::NowNanoseconds();
    bitmap_pending_slot_ = slot;
  }

  void ImageReady(CFbsBitmap* absl_nullable image, HBufC8* absl_nullable data,
                  TInt) override {
    delete image;
    delete data;
  }

  void FrameBufferReady(MFrameBuffer* absl_nullable buffer, TInt) override {
    if (buffer != nullptr) {
      buffer->Release();
    }
  }

 private:
  struct BitmapFrame {
    std::vector<unsigned char> pixels;
    int width = 0;
    int height = 0;
    int stride = 0;
    int format = 0;
    std::uint64_t sequence = 0;
    std::int64_t capture_time_ns = 0;
  };

  TInt SwitchToBitmapViewfinder() {
    if (!bitmap_supported_) {
      return KErrNotSupported;
    }
    NativeTrace("start: switch to bitmap viewfinder");
    camera_->PowerOff();
    camera_->Release();
    delete camera_;
    camera_ = nullptr;
    powered_ = false;
    reserved_ = false;
    const TInt connected = RFbsSession::Connect();
    if (connected != KErrNone) {
      return connected;
    }
    fbs_connected_ = true;
    legacy_mode_ = true;
    reserve_retry_count_ = 0;
    reserve_retry_at_ns_ = 0;
    TRAPD(error, camera_ = CCamera::NewL(static_cast<MCameraObserver&>(*this),
                                         index_));
    if (error != KErrNone) {
      return error;
    }
    camera_->Reserve();
    return KErrNone;
  }

  TInt StartViewFinder() {
    if (legacy_mode_) {
      for (int i = 0; i < request_count_; ++i) {
        bool duplicate = false;
        for (int earlier = 0; earlier < i; ++earlier) {
          duplicate |= requests_[earlier].width == requests_[i].width &&
                       requests_[earlier].height == requests_[i].height;
        }
        if (duplicate) {
          continue;
        }
        TSize selected(requests_[i].width, requests_[i].height);
        NativeTrace("start: bitmap StartViewFinder begin");
        TRAPD(error, camera_->StartViewFinderBitmapsL(selected));
        TBuf8<80> detail;
        detail.Format(
            _L8("start: bitmap request %dx%d result %d selected %dx%d"),
            requests_[i].width, requests_[i].height, error, selected.iWidth,
            selected.iHeight);
        NativeTrace(std::string_view(
            reinterpret_cast<const char*>(detail.Ptr()), detail.Length()));
        if (error == KErrNone) {
          size_ = selected;
          bitmap_started_ns_ =
              symbian::api::time::MonotonicClock::NowNanoseconds();
          viewfinder_started_ = true;
          return KErrNone;
        }
        if (error != KErrArgument && error != KErrNotSupported) {
          return error;
        }
      }
      return KErrNotSupported;
    }
    for (int i = 0; i < request_count_; ++i) {
      const NativeCameraRequest& request = requests_[i];
      TSize selected(request.width, request.height);
      if (request.format ==
          static_cast<int>(symbian::api::camera::PixelFormat::kBgr565)) {
        NativeTrace("start: format BGR565");
      } else if (request.format ==
                 static_cast<int>(
                     symbian::api::camera::PixelFormat::kRgbx8888)) {
        NativeTrace("start: format RGBX8888");
      } else {
        NativeTrace("start: other format");
      }
#if defined(SYMBIAN_CAMERA_NATIVE_TRACE)
      NativeTrace("start: leave probe begin");
      TRAPD(probe_error, User::Leave(KErrNotSupported));
      if (probe_error != KErrNotSupported) {
        NativeTrace("start: leave probe wrong result");
        return KErrCorrupt;
      }
      NativeTrace("start: leave probe passed");
#endif
      NativeTrace("start: StartViewFinder begin");
      TRAPD(start_error,
            camera_->StartViewFinderL(EcamFormat(request.format), selected));
      NativeTrace("start: StartViewFinder returned");
      if (start_error == KErrNone) {
        const bool yuv =
            request.format ==
            static_cast<int>(symbian::api::camera::PixelFormat::kYuv420Planar);
        if (selected.iWidth <= 0 || selected.iHeight <= 0 ||
            selected.iWidth > 8192 || selected.iHeight > 8192 ||
            (yuv &&
             ((selected.iWidth & 1) != 0 || (selected.iHeight & 1) != 0))) {
          camera_->StopViewFinder();
          return KErrCorrupt;
        }
        size_ = selected;
        format_ = request.format;
        viewfinder_started_ = true;
        return KErrNone;
      }
      if (start_error != KErrNotSupported && start_error != KErrArgument) {
        return start_error;
      }
    }
    return SwitchToBitmapViewfinder();
  }

  void OpenNativeTrace() {
#if defined(SYMBIAN_CAMERA_NATIVE_TRACE)
    if (trace_session_.Connect() != KErrNone) {
      return;
    }
    TBuf<80> path;
    path.Format(_L("E:\\Others\\camera_native_%08x.txt"),
                RProcess().SecureId().iId);
    if (trace_file_.Replace(trace_session_, path,
                            EFileWrite | EFileShareExclusive) == KErrNone) {
      trace_open_ = true;
    }
#endif
  }

  void NativeTrace(std::string_view message) {
#if defined(SYMBIAN_CAMERA_NATIVE_TRACE)
    if (!trace_open_) {
      return;
    }
    const TPtrC8 bytes(reinterpret_cast<const TUint8*>(message.data()),
                       static_cast<TInt>(message.size()));
    trace_file_.Write(bytes);
    _LIT8(KNewline, "\n");
    trace_file_.Write(KNewline);
    trace_file_.Flush();
#else
    (void)message;
#endif
  }

  void TraceDigitalZoom() {
#if defined(SYMBIAN_CAMERA_NATIVE_TRACE)
    TBuf8<48> detail;
    detail.Format(_L8("camera: zoom %d digital %d"), camera_->ZoomFactor(),
                  camera_->DigitalZoomFactor());
    NativeTrace(std::string_view(reinterpret_cast<const char*>(detail.Ptr()),
                                 detail.Length()));
#endif
  }

  void CloseNativeTrace() {
#if defined(SYMBIAN_CAMERA_NATIVE_TRACE)
    if (trace_open_) {
      trace_file_.Close();
      trace_open_ = false;
    }
    trace_session_.Close();
#endif
  }

  void ReleaseThreadInfrastructure() {
    if (installed_scheduler_) {
      CActiveScheduler::Install(previous_scheduler_);
      installed_scheduler_ = false;
    }
    delete cleanup_;
    cleanup_ = nullptr;
  }

  void ReserveFailed(TInt error) {
    if (error == KErrInUse && reserve_retry_count_ < 6) {
      NativeTrace("callback: camera busy; reserve retry pending");
      reserve_retry_at_ns_ =
          symbian::api::time::MonotonicClock::NowNanoseconds() + 500000000LL;
    } else {
      NativeTrace(error == KErrInUse
                      ? "callback: camera busy; reserve retries exhausted"
                      : "callback: reserve failed");
      error_ = error;
    }
  }

  TInt FailFrame(TInt error) {
    held_->Release();
    held_ = nullptr;
    error_ = error;
    return error;
  }

  const unsigned char* absl_nonnull ChunkPixelsL() {
    RChunk& chunk = held_->ChunkL();
    const TInt offset = held_->ChunkOffsetL(0);
    if (offset < 0 || offset > chunk.Size() - held_->FrameSize(0)) {
      User::Leave(KErrCorrupt);
    }
    return static_cast<const unsigned char*>(chunk.Base()) + offset;
  }

  const unsigned char* absl_nonnull DataPixelsL() {
    TDesC8* absl_nullable data = held_->DataL(0);
    if (data == nullptr || data->Length() < held_->FrameSize(0)) {
      User::Leave(KErrCorrupt);
    }
    return data->Ptr();
  }

  TSize size_;
  int index_ = 0;
  int format_;
  std::array<NativeCameraRequest, 8> requests_{};
  int request_count_ = 0;
  CTrapCleanup* absl_nullable cleanup_ = nullptr;
  CActiveScheduler* absl_nullable previous_scheduler_ = nullptr;
  CActiveScheduler scheduler_;
  CCamera* absl_nullable camera_ = nullptr;
  MCameraBuffer* absl_nullable pending_ = nullptr;
  MCameraBuffer* absl_nullable held_ = nullptr;
  std::array<BitmapFrame, 2> bitmap_frames_;
  int bitmap_pending_slot_ = -1;
  int bitmap_held_slot_ = -1;
  TInt error_ = KErrNone;
  int reserve_retry_count_ = 0;
  std::int64_t reserve_retry_at_ns_ = 0;
  std::uint64_t sequence_ = 0;
  int bitmap_callback_count_ = 0;
  std::optional<NativeCameraFrameConsumer> scoped_consumer_;
  void* absl_nullable scoped_context_ = nullptr;
  bool scoped_delivered_ = false;
  std::int64_t bitmap_started_ns_ = 0;
  std::int64_t capture_time_ns_ = 0;
  bool reserved_ = false;
  bool powered_ = false;
  bool start_pending_ = false;
  bool viewfinder_started_ = false;
  bool installed_scheduler_ = false;
  bool closed_ = false;
  bool bitmap_supported_ = false;
  bool legacy_mode_ = false;
  bool fbs_connected_ = false;
  std::uint32_t poll_count_ = 0;
#if defined(SYMBIAN_CAMERA_NATIVE_TRACE)
  RFs trace_session_;
  RFile trace_file_;
  bool trace_open_ = false;
#endif
};

}  // namespace

extern "C" int SymbianDeviceCameraStreamCreate(
    int index, const NativeCameraRequest* absl_nonnull requests,
    int request_count, void* absl_nullable* absl_nonnull state) {
  *state = nullptr;
  auto* absl_nullable camera =
      new (std::nothrow) CameraState(requests, request_count);
  if (camera == nullptr) {
    return KErrNoMemory;
  }
  const TInt result = camera->Open(index);
  if (result != KErrNone) {
    camera->Finalize();
    delete camera;
    return result;
  }
  *state = camera;
  return KErrNone;
}

extern "C" int SymbianDeviceCameraStreamPoll(
    void* absl_nonnull state, NativeCameraFrame* absl_nonnull frame) {
  return static_cast<CameraState*>(state)->Poll(frame);
}

extern "C" int SymbianDeviceCameraStreamPollScoped(
    void* absl_nonnull state, NativeCameraFrameConsumer consumer,
    void* absl_nonnull context, bool* absl_nonnull delivered) {
  return static_cast<CameraState*>(state)->PollScoped(consumer, context,
                                                      delivered);
}

extern "C" void SymbianDeviceCameraStreamClose(void* absl_nullable state) {
  if (state != nullptr) {
    static_cast<CameraState*>(state)->Stop();
  }
}

extern "C" void SymbianDeviceCameraFrameRelease(void* absl_nonnull owner) {
  static_cast<CameraState*>(owner)->ReleaseHeld();
}
