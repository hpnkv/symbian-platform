// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_API_CAMERA_NATIVE_CAMERA_STREAM_H_
#define SYMBIAN_API_CAMERA_NATIVE_CAMERA_STREAM_H_

#include <cstdint>

#include <absl/base/nullability.h>

struct NativeCameraFrame {
  const unsigned char* absl_nullable data = nullptr;
  int bytes = 0;
  int width = 0;
  int height = 0;
  int stride = 0;
  int format = 0;
  bool mapped = false;
  std::uint64_t sequence = 0;
  std::int64_t capture_time_ns = 0;
  void* absl_nullable release_owner = nullptr;
};

struct NativeCameraRequest {
  int width = 0;
  int height = 0;
  int format = 0;
};

using NativeCameraFrameConsumer =
    void (* absl_nonnull)(const NativeCameraFrame* absl_nonnull,
                           void* absl_nonnull);

extern "C" int SymbianDeviceCameraStreamCreate(
    int index, const NativeCameraRequest* absl_nonnull requests,
    int request_count, void* absl_nullable* absl_nonnull state);
extern "C" int SymbianDeviceCameraStreamPoll(
    void* absl_nonnull state, NativeCameraFrame* absl_nonnull frame);
extern "C" int SymbianDeviceCameraStreamPollScoped(
    void* absl_nonnull state, NativeCameraFrameConsumer consumer,
    void* absl_nonnull context, bool* absl_nonnull delivered);
extern "C" void SymbianDeviceCameraStreamClose(void* absl_nullable state);
extern "C" void SymbianDeviceCameraFrameRelease(void* absl_nonnull owner);

#endif  // SYMBIAN_API_CAMERA_NATIVE_CAMERA_STREAM_H_
