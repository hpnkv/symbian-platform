// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "symbian/api/system/failure_handler.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <absl/base/nullability.h>

#include "symbian/api/display/window_surface.h"
#include "symbian/api/storage/storage.h"
#include "symbian/api/system/clipboard.h"
#include "symbian/api/system/debug_log.h"
#include "symbian/api/text/utf8.h"
#include "symbian/api/time/sleep.h"

namespace symbian::api::system {
namespace {

namespace display = symbian::api::display;
namespace storage = symbian::api::storage;

constexpr std::size_t kRecentLogBytes = 16 * 1024;
constexpr int kMargin = 16;
constexpr int kHeaderHeight = 72;
constexpr int kFooterHeight = 80;
constexpr std::uint16_t kBackground = 0x10c4;
constexpr std::uint16_t kPanel = 0x2148;
constexpr std::uint16_t kExitButton = 0x6188;
constexpr std::uint16_t kCopyButton = 0x3489;

void Fill(display::Rgb565Frame* absl_nonnull frame, int left, int top,
          int right, int bottom, std::uint16_t color) {
  left = std::clamp(left, 0, frame->size.width);
  right = std::clamp(right, 0, frame->size.width);
  top = std::clamp(top, 0, frame->size.height);
  bottom = std::clamp(bottom, 0, frame->size.height);
  for (int y = top; y < bottom; ++y) {
    for (int x = left; x < right; ++x) {
      std::byte* absl_nonnull pixel =
          frame->pixels.data() + y * frame->pitch_bytes + x * 2;
      pixel[0] = static_cast<std::byte>(color & 255);
      pixel[1] = static_cast<std::byte>(color >> 8);
    }
  }
}

std::string ReadLogTail(std::u16string_view path) {
  auto file = storage::ReadOnlyFile::Open(path);
  if (!file.ok()) {
    return "[log unavailable: " + file.status().ToString() + "]\n";
  }
  auto size = file->Size();
  if (!size.ok()) {
    return "[log size unavailable: " + size.status().ToString() + "]\n";
  }
  const std::size_t requested =
      static_cast<std::size_t>(std::min<std::uint64_t>(*size, kRecentLogBytes));
  if (requested == 0) {
    return "[empty log]\n";
  }
  std::vector<std::byte> bytes(requested);
  auto read = file->ReadAt(*size - requested, bytes);
  if (!read.ok()) {
    return "[log read failed: " + read.status().ToString() + "]\n";
  }
  const auto* absl_nonnull characters =
      reinterpret_cast<const char*>(bytes.data());
  return std::string(characters, *read);
}

std::string BuildReport(const absl::Status& error,
                        const FailureHandlerOptions& options) {
  std::string report = "ERROR\n" + error.ToString() + "\n\nRECENT LOGS\n";
  std::string recent(kRecentLogBytes, '\0');
  recent.resize(
      CopyRecentDebugLogs(std::span<char>(recent.data(), recent.size())));
  report += recent.empty() ? "[no recent DebugLog messages]\n" : recent;
  for (std::u16string_view path : options.log_paths) {
    report += "\nFILE: ";
    auto name = text::Utf16ToUtf8(path);
    report += name.ok() ? *name : "[invalid path]";
    report += '\n';
    report += ReadLogTail(path);
  }
  return report;
}

std::u16string DisplayText(std::string_view source) {
  if (auto converted = text::Utf8ToUtf16(source); converted.ok()) {
    return std::move(*converted);
  }
  std::u16string fallback;
  fallback.reserve(source.size());
  for (unsigned char byte : source) {
    fallback.push_back((byte >= 32 && byte < 127) || byte == '\n' ||
                               byte == '\r' || byte == '\t'
                           ? byte
                           : u'\ufffd');
  }
  return fallback;
}

int FontHeight(display::WindowSize size) {
  return size.width >= 600 ? 24 : (size.width >= 300 ? 18 : 14);
}

int VisibleRows(display::WindowSize size, int line_height) {
  return std::max(
      1, (size.height - kHeaderHeight - kFooterHeight - 20) / line_height);
}

int MaxScroll(std::size_t lines, int rows) {
  return std::max(0, static_cast<int>(lines) - rows);
}

absl::Status Render(display::WindowSurface* absl_nonnull window,
                    display::Rgb565Frame* absl_nonnull frame,
                    std::u16string_view caption,
                    const std::vector<std::u16string>& lines, int scroll,
                    std::u16string_view copy_feedback) {
  const display::WindowSize size = frame->size;
  const int font_height = FontHeight(size);
  const int line_height = font_height + 8;
  const int visible = VisibleRows(size, line_height);
  const int footer_top = size.height - kFooterHeight;
  const int middle = size.width / 2;
  Fill(frame, 0, 0, size.width, size.height, kBackground);
  Fill(frame, 0, 0, size.width, kHeaderHeight, kExitButton);
  Fill(frame, kMargin, kHeaderHeight + 8, size.width - kMargin, footer_top - 8,
       kPanel);
  Fill(frame, kMargin, footer_top + 8, middle - 5, size.height - 10,
       kExitButton);
  Fill(frame, middle + 5, footer_top + 8, size.width - kMargin,
       size.height - 10, kCopyButton);
  if (static_cast<int>(lines.size()) > visible) {
    const int track_top = kHeaderHeight + 12;
    const int track_height = std::max(1, footer_top - track_top - 24);
    const int thumb_height =
        std::max(12, track_height * visible / static_cast<int>(lines.size()));
    const int thumb_top = track_top + (track_height - thumb_height) * scroll /
                                          MaxScroll(lines.size(), visible);
    Fill(frame, size.width - kMargin - 5, track_top, size.width - kMargin - 2,
         track_top + track_height, 0x8410);
    Fill(frame, size.width - kMargin - 7, thumb_top, size.width - kMargin,
         thumb_top + thumb_height, 0xffff);
  }
  if (absl::Status presented = window->Present(); !presented.ok()) {
    return presented;
  }
  std::vector<display::WindowTextLine> text_lines;
  text_lines.reserve(static_cast<std::size_t>(visible) + 4);
  text_lines.push_back(
      {.text = caption, .x = kMargin + 8, .baseline_y = 42, .rgb = 0xffffff});
  for (int row = 0;
       row < visible && scroll + row < static_cast<int>(lines.size()); ++row) {
    text_lines.push_back(
        {.text = lines[scroll + row],
         .x = kMargin + 8,
         .baseline_y = kHeaderHeight + 18 + (row + 1) * line_height,
         .rgb = 0xf7f7f7});
  }
  text_lines.push_back({.text = u"EXIT",
                        .x = kMargin + 18,
                        .baseline_y = footer_top + 46,
                        .rgb = 0xffffff});
  text_lines.push_back(
      {.text = copy_feedback.empty() ? u"COPY LOGS" : copy_feedback,
       .x = middle + 18,
       .baseline_y = footer_top + 46,
       .rgb = 0xffffff});
  return window->DrawTextLines(text_lines, font_height);
}

}  // namespace

absl::Status ShowFailureReport(const absl::Status& error,
                               const FailureHandlerOptions& options) {
  if (error.ok()) {
    return absl::InvalidArgumentError("failure report needs an error");
  }
  const std::string report = BuildReport(error, options);
  const std::u16string report_text = DisplayText(report);
  std::u16string caption = DisplayText(options.caption);
  if (caption.empty()) {
    caption = u"Application failure";
  }
  display::WindowSurface window;
  if (absl::Status opened = window.Open(options.caption); !opened.ok()) {
    return opened;
  }
  auto created = window.CreateRgb565Frame();
  if (!created.ok()) {
    return created.status();
  }
  display::Rgb565Frame frame = *created;
  const int font_height = FontHeight(frame.size);
  const int line_height = font_height + 8;
  auto wrapped = window.WrapTextLines(
      report_text, std::max(1, frame.size.width - 2 * kMargin - 26),
      font_height);
  if (!wrapped.ok()) {
    return wrapped.status();
  }
  std::vector<std::u16string> lines = std::move(*wrapped);
  int scroll = 0;
  int touch_start_y = 0;
  int touch_start_scroll = 0;
  int pressed_button = -1;
  bool dragging = false;
  bool redraw = true;
  std::u16string feedback;
  for (;;) {
    if (redraw) {
      if (absl::Status result =
              Render(&window, &frame, caption, lines, scroll, feedback);
          !result.ok()) {
        return result;
      }
      redraw = false;
    }
    for (int count = 0; count < 64; ++count) {
      auto next = window.PollInput();
      if (!next.ok()) {
        return next.status();
      }
      if (!next->has_value()) {
        break;
      }
      const display::WindowInput& input = **next;
      const int maximum =
          MaxScroll(lines.size(), VisibleRows(frame.size, line_height));
      switch (input.kind) {
        case display::WindowInputKind::kPointerDown:
          pressed_button = input.y >= frame.size.height - kFooterHeight
                               ? (input.x < frame.size.width / 2 ? 0 : 1)
                               : -1;
          dragging = pressed_button < 0;
          touch_start_y = input.y;
          touch_start_scroll = scroll;
          break;
        case display::WindowInputKind::kPointerMove:
          if (dragging) {
            const int next_scroll = std::clamp(
                touch_start_scroll + (touch_start_y - input.y) / line_height, 0,
                maximum);
            redraw |= next_scroll != scroll;
            scroll = next_scroll;
          }
          break;
        case display::WindowInputKind::kPointerUp:
          dragging = false;
          if (pressed_button == 0 &&
              input.y >= frame.size.height - kFooterHeight &&
              input.x < frame.size.width / 2) {
            return absl::OkStatus();
          }
          if (pressed_button == 1 &&
              input.y >= frame.size.height - kFooterHeight &&
              input.x >= frame.size.width / 2) {
            absl::Status copied = CopyTextToClipboard(report_text);
            feedback = copied.ok() ? u"COPIED" : u"COPY FAILED";
            redraw = true;
          }
          pressed_button = -1;
          break;
        case display::WindowInputKind::kKeyDown:
          if (input.key == display::WindowKey::kEscape ||
              input.key == display::WindowKey::kBackspace) {
            return absl::OkStatus();
          }
          if (input.key == display::WindowKey::kSelect ||
              input.key == display::WindowKey::kEnter) {
            absl::Status copied = CopyTextToClipboard(report_text);
            feedback = copied.ok() ? u"COPIED" : u"COPY FAILED";
            redraw = true;
          } else if (input.key == display::WindowKey::kUp) {
            scroll = std::max(0, scroll - 1);
            redraw = true;
          } else if (input.key == display::WindowKey::kDown) {
            scroll = std::min(maximum, scroll + 1);
            redraw = true;
          }
          break;
        case display::WindowInputKind::kCloseRequested:
          return absl::OkStatus();
        case display::WindowInputKind::kDisplayChanged:
          window.DestroyFrame();
          created = window.CreateRgb565Frame();
          if (!created.ok()) {
            return created.status();
          }
          frame = *created;
          scroll = 0;
          wrapped = window.WrapTextLines(
              report_text, std::max(1, frame.size.width - 2 * kMargin - 26),
              FontHeight(frame.size));
          if (!wrapped.ok()) {
            return wrapped.status();
          }
          lines = std::move(*wrapped);
          redraw = true;
          break;
        case display::WindowInputKind::kFocusGained:
          redraw = true;
          break;
        default:
          break;
      }
    }
    symbian::api::time::SleepFor(std::chrono::milliseconds(30));
  }
}

int RunWithFailureHandler(FailureHandledEntry entry,
                          const FailureHandlerOptions& options) {
  if (!entry) {
    ShowFailureReport(absl::InvalidArgumentError("Failure entry is empty"),
                      options)
        .IgnoreError();
    return 1;
  }
  const absl::Status result = entry();
  if (result.ok()) {
    return 0;
  }
  ShowFailureReport(result, options).IgnoreError();
  return 1;
}

}  // namespace symbian::api::system
