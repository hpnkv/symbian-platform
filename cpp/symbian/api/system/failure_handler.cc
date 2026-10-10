// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "symbian/api/system/failure_handler.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <absl/base/nullability.h>
#include <absl/status/status_macros.h>
#include <e32std.h>
#include <w32std.h>

#include "../display/foreground_state.h"
#include "absl/base/config.h"
#include "absl/base/no_destructor.h"
#include "absl/log/initialize.h"
#include "absl/log/log_entry.h"
#include "absl/log/log_sink.h"
#include "absl/log/log_sink_registry.h"
#include "symbian/api/display/window_surface.h"
#include "symbian/api/storage/storage.h"
#include "symbian/api/system/clipboard.h"
#include "symbian/api/system/debug_log.h"
#include "symbian/api/text/utf8.h"

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
std::atomic_flag reporting_fatal = ATOMIC_FLAG_INIT;

class SdkLogSink final : public absl::LogSink {
 public:
  void Send(const absl::LogEntry& entry) override {
    const absl::string_view message = entry.text_message_with_prefix();
    DebugLog(std::string_view(message.data(), message.size()));
  }
};

[[maybe_unused]] const bool logging_registered = [] {
  static absl::NoDestructor<SdkLogSink> sink;
  absl::InitializeLog();
  absl::AddLogSink(sink.get());
  return true;
}();

std::u16string FailurePath() {
  std::u16string path = u"C:\\private\\";
  const std::uint32_t uid = RProcess().SecureId().iId;
  constexpr char16_t hex[] = u"0123456789abcdef";
  for (int shift = 28; shift >= 0; shift -= 4) {
    path.push_back(hex[(uid >> shift) & 15]);
  }
  path += u"\\failure.txt";
  return path;
}

void PersistReport(std::string_view report) {
  const std::u16string path = FailurePath();
  const std::u16string directory = path.substr(0, path.rfind(u'\\'));
  storage::CreateDirectories(directory).IgnoreError();
  auto file =
      storage::WritableFile::Open(path, storage::WriteMode::kReplaceExisting);
  if (!file.ok()) {
    return;
  }
  const auto* absl_nonnull bytes =
      reinterpret_cast<const std::byte*>(report.data());
  if (file->WriteAt(0, std::span<const std::byte>(bytes, report.size())).ok()) {
    file->Flush().IgnoreError();
  }
}

bool CurrentProcessOwnsFocusedWindow() {
  RWsSession session;
  if (session.Connect() != KErrNone) {
    return false;
  }
  TThreadId owner;
  const TInt focused = session.GetFocusWindowGroup();
  const TInt result = focused > 0
                          ? session.GetWindowGroupClientThreadId(focused, owner)
                          : KErrNotFound;
  session.Close();
  if (result != KErrNone) {
    return false;
  }
  if (owner == RThread().Id()) {
    return true;
  }
  RThread thread;
  if (thread.Open(owner) != KErrNone) {
    return false;
  }
  RProcess process;
  const TInt opened = thread.Process(process);
  thread.Close();
  if (opened != KErrNone) {
    return false;
  }
  const bool same_process = process.Id() == RProcess().Id();
  process.Close();
  return same_process;
}

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
  for (const std::u16string_view path : options.log_paths) {
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
  for (const unsigned char byte : source) {
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
      1, (size.height - kHeaderHeight - kFooterHeight - 24) / line_height);
}

int MaxScroll(std::size_t lines, display::WindowSize size, int line_height) {
  const int viewport =
      std::max(1, size.height - kHeaderHeight - kFooterHeight - 24);
  return std::max(0, static_cast<int>(lines) * line_height - viewport);
}

absl::Status Render(display::WindowSurface* absl_nonnull window,
                    std::u16string_view caption,
                    const std::vector<std::u16string>& lines, int scroll,
                    std::u16string_view copy_feedback) {
  const display::WindowSize size = window->size();
  const int font_height = FontHeight(size);
  const int line_height = font_height + 8;
  const int visible = VisibleRows(size, line_height);
  const int first_line = scroll / line_height;
  const int remainder = scroll % line_height;
  const int footer_top = size.height - kFooterHeight;
  const int middle = size.width / 2;
  // Paint directly into the native bitmap while its heap is locked. No
  // persistent writable view, staging chunk or frame-size copy is needed.
  ABSL_RETURN_IF_ERROR(window->UpdateRgb565Frame(
      {.write = [&](display::Rgb565Frame frame) -> absl::Status {
        Fill(&frame, 0, 0, size.width, size.height, kBackground);
        Fill(&frame, 0, 0, size.width, kHeaderHeight, kExitButton);
        Fill(&frame, kMargin, kHeaderHeight + 8, size.width - kMargin,
             footer_top - 8, kPanel);
        Fill(&frame, kMargin, footer_top + 8, middle - 5, size.height - 10,
             kExitButton);
        Fill(&frame, middle + 5, footer_top + 8, size.width - kMargin,
             size.height - 10, kCopyButton);
        if (MaxScroll(lines.size(), size, line_height) > 0) {
          const int track_top = kHeaderHeight + 12;
          const int track_height = std::max(1, footer_top - track_top - 24);
          const int thumb_height = std::max(
              12, track_height * visible / static_cast<int>(lines.size()));
          const int thumb_top =
              track_top +
              static_cast<std::int64_t>(track_height - thumb_height) * scroll /
                  MaxScroll(lines.size(), size, line_height);
          Fill(&frame, size.width - kMargin - 5, track_top,
               size.width - kMargin - 2, track_top + track_height, 0x8410);
          Fill(&frame, size.width - kMargin - 7, thumb_top,
               size.width - kMargin, thumb_top + thumb_height, 0xffff);
        }
        return absl::OkStatus();
      }}));
  std::vector<display::WindowTextLine> text_lines;
  text_lines.reserve(static_cast<std::size_t>(visible) + 6);
  text_lines.push_back(
      {.text = caption, .x = kMargin + 8, .baseline_y = 42, .rgb = 0xffffff});
  for (int row = 0;
       row < visible + 2 && first_line + row < static_cast<int>(lines.size());
       ++row) {
    text_lines.push_back({.text = lines[first_line + row],
                          .x = kMargin + 8,
                          .baseline_y = kHeaderHeight + 12 + font_height +
                                        row * line_height - remainder,
                          .rgb = 0xf7f7f7,
                          .clip = display::WindowRect{
                              .x = kMargin + 8,
                              .y = kHeaderHeight + 12,
                              .width = size.width - 2 * kMargin - 18,
                              .height = footer_top - kHeaderHeight - 24}});
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
  return window->Present(text_lines, font_height);
}

}  // namespace

absl::Status ShowFailureReport(const absl::Status& error,
                               const FailureHandlerOptions& options) {
  if (error.ok()) {
    return absl::InvalidArgumentError("failure report needs an error");
  }
  const std::string report = BuildReport(error, options);
  PersistReport(report);
  const std::u16string report_text = DisplayText(report);
  std::u16string caption = DisplayText(options.caption);
  if (caption.empty()) {
    caption = u"Application failure";
  }
  ABSL_ASSIGN_OR_RETURN(auto window,
                        display::WindowSurface::Create(options.caption));
  display::WindowSize size = window.size();
  int font_height = FontHeight(size);
  int line_height = font_height + 8;
  const int text_width = std::max(1, size.width - 2 * kMargin - 26);
  ABSL_ASSIGN_OR_RETURN(
      auto lines, window.WrapTextLines(report_text, text_width, font_height));
  int scroll = 0;
  int touch_start_y = 0;
  int touch_start_scroll = 0;
  int pressed_button = -1;
  bool dragging = false;
  bool redraw = true;
  std::u16string feedback;
  for (;;) {
    // Consume input before painting, so a drag is visible in this frame rather
    // than waiting through another display-pacing interval.
    for (int count = 0; count < 64; ++count) {
      ABSL_ASSIGN_OR_RETURN(auto next, window.PollInput());
      if (!next.has_value()) {
        break;
      }
      const display::WindowInput& input = *next;
      const int maximum = MaxScroll(lines.size(), size, line_height);
      switch (input.kind) {
        case display::WindowInputKind::kPointerDown:
          pressed_button = input.y >= size.height - kFooterHeight
                               ? (input.x < size.width / 2 ? 0 : 1)
                               : -1;
          dragging = pressed_button < 0;
          touch_start_y = input.y;
          touch_start_scroll = scroll;
          break;
        case display::WindowInputKind::kPointerMove:
          if (dragging) {
            // Pointer coordinates and scroll are both screen pixels. Preserve
            // their distance directly, without a row threshold or gain curve.
            const int next_scroll = std::clamp(
                touch_start_scroll + (touch_start_y - input.y), 0, maximum);
            redraw |= next_scroll != scroll;
            scroll = next_scroll;
          }
          break;
        case display::WindowInputKind::kPointerUp:
          dragging = false;
          if (pressed_button == 0 && input.y >= size.height - kFooterHeight &&
              input.x < size.width / 2) {
            return absl::OkStatus();
          }
          if (pressed_button == 1 && input.y >= size.height - kFooterHeight &&
              input.x >= size.width / 2) {
            const absl::Status copied = CopyTextToClipboard(report_text);
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
            const absl::Status copied = CopyTextToClipboard(report_text);
            feedback = copied.ok() ? u"COPIED" : u"COPY FAILED";
            redraw = true;
          } else if (input.key == display::WindowKey::kUp) {
            scroll = std::max(0, scroll - line_height);
            redraw = true;
          } else if (input.key == display::WindowKey::kDown) {
            scroll = std::min(maximum, scroll + line_height);
            redraw = true;
          }
          break;
        case display::WindowInputKind::kCloseRequested:
          return absl::OkStatus();
        case display::WindowInputKind::kDisplayChanged: {
          window.DestroyFrame();
          size = window.size();
          scroll = 0;
          dragging = false;
          pressed_button = -1;
          font_height = FontHeight(size);
          line_height = font_height + 8;
          ABSL_ASSIGN_OR_RETURN(
              lines,
              window.WrapTextLines(report_text,
                                   std::max(1, size.width - 2 * kMargin - 26),
                                   FontHeight(size)));
          redraw = true;
          break;
        }
        case display::WindowInputKind::kFocusLost:
          dragging = false;
          pressed_button = -1;
          break;
        case display::WindowInputKind::kFocusGained:
          redraw = true;
          break;
        default:
          break;
      }
    }
    if (redraw) {
      ABSL_RETURN_IF_ERROR(Render(&window, caption, lines, scroll, feedback));
      redraw = false;
    }
    // Wake on the next pointer event, rather than sleeping through part of a
    // frame after each draw. Static reports consume no periodic wakeups.
    ABSL_RETURN_IF_ERROR(window.WaitForInput());
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

// The application link roots this object even when only CHECK/LOG macros are
// used. A named anchor avoids whole-archive duplication in dependency graphs.
extern "C" void symbian_sdk_failure_handler_link_anchor() {}

// Abseil calls this weak extension point before it aborts for CHECK and
// CHECK_OK. Its strong definition is linked into each SDK application.
extern "C" void ABSL_INTERNAL_C_SYMBOL(AbslInternalOnFatalLogMessage)(
    const absl::LogEntry& entry) {
  using symbian::api::system::ShowFailureReport;
  if (symbian::api::system::reporting_fatal.test_and_set(
          std::memory_order_acq_rel)) {
    RProcess().Kill(1);
    User::Exit(1);
  }
  const absl::string_view message = entry.text_message_with_prefix();
  const absl::Status failure =
      absl::InternalError(std::string(message.data(), message.size()));
  const bool foreground =
      symbian::api::system::CurrentProcessOwnsFocusedWindow() ||
      symbian::api::display::internal::LastWindowClosedInForeground();
  if (foreground) {
    ShowFailureReport(failure).IgnoreError();
  } else {
    symbian::api::system::PersistReport(
        symbian::api::system::BuildReport(failure, {}));
  }
  RProcess().Kill(1);
  User::Exit(1);
}
