// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_API_DISPLAY_WINDOW_SURFACE_H_
#define SYMBIAN_API_DISPLAY_WINDOW_SURFACE_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace symbian::api::display {

struct WindowSize {
  int width = 0;
  int height = 0;
};

// Rotation of the current display mode relative to its native orientation.
enum class DisplayRotation { k0, k90, k180, k270 };

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

// The callable runs during one synchronous write. The bitmap view is valid
// only in the callback; the SDK unlocks it before presentation.
struct Rgb565FrameWriter {
  std::function<absl::Status(Rgb565Frame)> write;
};

// A window-local clipping rectangle. Pixels outside it are unchanged.
struct WindowRect {
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
};

// A native-font label drawn over the most recently presented frame.
struct WindowTextLine {
  std::u16string_view text;
  int x = 0;
  int baseline_y = 0;
  std::uint32_t rgb = 0xffffff;
  std::optional<WindowRect> clip;
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
  static absl::StatusOr<WindowSurface> Create(
      std::string_view task_caption = {});
  static absl::StatusOr<std::unique_ptr<WindowSurface>> CreateUnique(
      std::string_view task_caption = {});
  WindowSurface(const WindowSurface&) = delete;
  WindowSurface& operator=(const WindowSurface&) = delete;
  WindowSurface(WindowSurface&& other) noexcept;
  WindowSurface& operator=(WindowSurface&& other) noexcept;
  ~WindowSurface();

  static absl::StatusOr<WindowSize> PrimarySize();
  /** @brief Physical refresh rate when the display driver reports one. */
  static std::optional<std::uint32_t> PrimaryRefreshRateHz();
  /** @brief Follow the device's automatic display orientation policy. */
  absl::Status SetAutomaticOrientation(bool enabled);
  absl::StatusOr<Rgb565Frame> CreateRgb565Frame();
  absl::Status UpdateRgb565Frame(Rgb565FrameWriter writer);
  absl::Status Present();
  // Composes clipped native text into the bitmap before a single window blit.
  // Window Server chooses the platform rendering backend; this needs neither
  // EGL nor a GPU on older devices.
  absl::Status Present(std::span<const WindowTextLine> lines,
                       int font_height_pixels = 20);
  // Composes labels into the current RGB565 bitmap and presents it. Replace
  // the frame's pixels before replacing labels. Redraws replay the composed
  // bitmap; the caller owns text until this returns.
  absl::Status DrawTextLines(std::span<const WindowTextLine> lines,
                             int font_height_pixels = 20);
  // Wraps UTF-16 text to the actual device font metrics, preserving newlines.
  absl::StatusOr<std::vector<std::u16string>> WrapTextLines(
      std::u16string_view text, int max_width_pixels,
      int font_height_pixels = 20) const;
  absl::StatusOr<std::optional<WindowInput>> PollInput();
  void DestroyFrame();
  void Close();

  WindowSize size() const;
  DisplayRotation rotation() const;
  /** @brief Opaque native window for the SDK EGL compatibility bridge. */
  void* absl_nullable NativeWindowHandle() const;
  void set_measure_frames(bool enabled);
  WindowFrameMetrics frame_metrics() const;

 private:
  WindowSurface();
  absl::Status Open(std::string_view task_caption);
  absl::Status ValidateTextLines(std::span<const WindowTextLine> lines,
                                 int font_height_pixels) const;
  struct Impl;
  Impl* absl_nullable impl_ = nullptr;
};

}  // namespace symbian::api::display

#endif  // SYMBIAN_API_DISPLAY_WINDOW_SURFACE_H_
