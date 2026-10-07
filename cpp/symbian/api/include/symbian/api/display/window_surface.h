// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_API_DISPLAY_WINDOW_SURFACE_H_
#define SYMBIAN_API_DISPLAY_WINDOW_SURFACE_H_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace symbian::api::display {

struct WindowSize {
  int width = 0;
  int height = 0;
};

enum class WindowInputKind {
  kPointerMove,
  kPointerDown,
  kPointerUp,
  kKeyDown,
  kKeyUp,
  kFocusGained,
  kFocusLost,
  kCloseRequested,
  kDisplayChanged,
};

enum class WindowKey {
  kUnknown,
  kEscape,
  kBackspace,
  kLeft,
  kRight,
  kUp,
  kDown,
  kEnter,
  kSelect,
};

struct WindowInput {
  WindowInputKind kind = WindowInputKind::kPointerMove;
  int x = 0;
  int y = 0;
  WindowKey key = WindowKey::kUnknown;
};

/** @brief A live RGB565 bitmap view owned by its WindowSurface. */
struct Rgb565Frame {
  std::span<std::byte> pixels;
  int pitch_bytes = 0;
  WindowSize size;
};

/** @brief Optional per-window presentation counters for device comparisons. */
struct WindowFrameMetrics {
  std::uint64_t frames = 0;
  std::uint64_t total_native_ticks = 0;
  std::uint32_t max_native_ticks = 0;
};

/**
 * @brief Owns one Window Server window, its requests and optional RGB565 frame.
 *
 * All methods run on the opening thread. PollInput is nonblocking; the caller
 * owns pacing and any structured task scope. The frame view remains valid until
 * DestroyFrame or Close. At most one window and one bitmap are allocated.
 */
class WindowSurface final {
 public:
  WindowSurface();
  WindowSurface(const WindowSurface&) = delete;
  WindowSurface& operator=(const WindowSurface&) = delete;
  WindowSurface(WindowSurface&& other) noexcept;
  WindowSurface& operator=(WindowSurface&& other) noexcept;
  ~WindowSurface();

  static absl::StatusOr<WindowSize> PrimarySize();
  /** @brief Physical refresh rate when the display driver reports one. */
  static std::optional<std::uint32_t> PrimaryRefreshRateHz();
  /** @brief Open a task-listed window with the process UID and caption. */
  absl::Status Open(std::string_view task_caption = {});
  absl::StatusOr<Rgb565Frame> CreateRgb565Frame();
  absl::Status Present();
  absl::StatusOr<std::optional<WindowInput>> PollInput();
  void DestroyFrame();
  void Close();

  WindowSize size() const;
  /** @brief Opaque native window for the SDK EGL compatibility bridge. */
  void* absl_nullable NativeWindowHandle() const;
  void set_measure_frames(bool enabled);
  WindowFrameMetrics frame_metrics() const;

 private:
  struct Impl;
  Impl* absl_nullable impl_ = nullptr;
};

}  // namespace symbian::api::display

#endif  // SYMBIAN_API_DISPLAY_WINDOW_SURFACE_H_
