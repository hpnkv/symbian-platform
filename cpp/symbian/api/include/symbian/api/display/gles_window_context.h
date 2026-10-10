// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_API_DISPLAY_GLES_WINDOW_CONTEXT_H_
#define SYMBIAN_API_DISPLAY_GLES_WINDOW_CONTEXT_H_

#include <cstdint>
#include <string_view>

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "symbian/api/display/window_surface.h"

namespace symbian::api::display {

/** @brief Timing counters for EGL presentation on one window. */
struct GlesFrameMetrics {
  std::uint64_t frames = 0;
  std::uint64_t total_native_ticks = 0;
  std::uint32_t max_native_ticks = 0;
};

/** @brief Minimum channel and depth bits requested from the EGL driver. */
struct GlesContextFormat {
  int red_bits = 5;
  int green_bits = 6;
  int blue_bits = 5;
  int depth_bits = 0;
};

/**
 * @brief Owns an OpenGL ES context and EGL surface for a WindowSurface.
 *
 * The window must outlive the context. All methods run on the opening thread.
 * Open fails when the deployment firmware has no suitable EGL configuration;
 * callers can then use WindowSurface's RGB565 software frame instead.
 */
class GlesWindowContext final {
 public:
  using Procedure = void (*absl_nullable)(...);

  GlesWindowContext();
  GlesWindowContext(const GlesWindowContext&) = delete;
  GlesWindowContext& operator=(const GlesWindowContext&) = delete;
  ~GlesWindowContext();

  absl::Status Open(WindowSurface* absl_nonnull window, int major_version,
                    GlesContextFormat format = {});
  absl::Status MakeCurrent();
  absl::Status ClearCurrent();
  // Recreate the native EGL surface after the window changes geometry while
  // retaining the GL context and its textures.
  absl::Status RefreshSurface();
  // Actual drawable size reported by EGL. This may lag a native window resize
  // until RefreshSurface recreates the surface.
  absl::StatusOr<WindowSize> SurfaceSize() const;
  absl::Status Swap();
  absl::Status SetSwapInterval(int interval);
  int swap_interval() const;
  bool is_open() const;
  void set_measure_frames(bool enabled);
  GlesFrameMetrics frame_metrics() const;
  void Close();

  static Procedure absl_nullable GetProcAddress(std::string_view name);

 private:
  struct Impl;
  Impl* absl_nullable impl_ = nullptr;
};

}  // namespace symbian::api::display

#endif  // SYMBIAN_API_DISPLAY_GLES_WINDOW_CONTEXT_H_
