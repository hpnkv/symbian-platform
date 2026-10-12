// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include <algorithm>
#include <cstddef>
#include <span>
#include <string_view>

#include <absl/status/status_macros.h>

#include "absl/status/status.h"
#include "symbian/api/display/window_surface.h"
#include "symbian/api/storage/storage.h"
#include "symbian/api/system/debug_log.h"
#include "symbian/api/system/failure_report_tasks.h"
#include "symbian/api/system/failure_view_thread.h"
#include "symbian/concurrency/future.h"
#include "thread/boost_primitives.h"

namespace {

namespace system_api = symbian::api::system;
namespace storage = symbian::api::storage;
namespace display = symbian::api::display;
using symbian::concurrency::Promise;
using symbian::concurrency::Unit;

constexpr std::u16string_view kDirectory = u"C:\\private\\e0000a73";
constexpr std::u16string_view kPath = u"C:\\private\\e0000a73\\failure.txt";
constexpr std::string_view kExpected = "failure report survived UI exit\n";

absl::Status Save(std::string_view text) {
  ABSL_RETURN_IF_ERROR(storage::CreateDirectories(kDirectory));
  ABSL_ASSIGN_OR_RETURN(
      auto file,
      storage::WritableFile::Open(kPath, storage::WriteMode::kReplaceExisting));
  ABSL_RETURN_IF_ERROR(file.WriteAt(
      0, std::as_bytes(std::span<const char>(text.data(), text.size()))));
  return file.Flush();
}

absl::Status Present(Promise<Unit>* absl_nonnull opened) {
  ABSL_ASSIGN_OR_RETURN(
      auto window, display::WindowSurface::Create("Failure task lifecycle"));
  ABSL_RETURN_IF_ERROR(
      window.UpdateRgb565Frame({.write = [](display::Rgb565Frame frame) {
        // A distinct RGB565 background makes window lifetime visible in captures.
        for (std::size_t index = 0; index + 1 < frame.pixels.size();
             index += 2) {
          frame.pixels[index] = std::byte{0xe0};
          frame.pixels[index + 1] = std::byte{0x07};
        }
        return absl::OkStatus();
      }}));
  ABSL_RETURN_IF_ERROR(window.Present());
  opened->SetValue(Unit{});
  system_api::DebugLog("FA:UI_READY");
  for (;;) {
    ABSL_ASSIGN_OR_RETURN(auto input, window.PollInput());
    if (input && (input->kind == display::WindowInputKind::kCloseRequested ||
                  (input->kind == display::WindowInputKind::kKeyDown &&
                   input->key == display::WindowKey::kEscape))) {
      return absl::OkStatus();
    }
    ABSL_RETURN_IF_ERROR(window.WaitForInput());
  }
}

absl::Status Run() {
  // A background failure needs no UI worker but must finish its write.
  ABSL_RETURN_IF_ERROR(system_api::internal::RunFailureReportTasks(
      {}, [] { return Save("background report\n"); }));

  // A failed UI is not a cancellation signal for persistence.
  Promise<Unit> failed;
  bool persisted = false;
  const absl::Status unavailable = system_api::internal::RunFailureReportTasks(
      [&] {
        failed.SetValue(Unit{});
        return absl::UnavailableError("Injected presentation failure");
      },
      [&]() -> absl::Status {
        ABSL_RETURN_IF_ERROR(failed.future().Await().status());
        ABSL_RETURN_IF_ERROR(Save("report survived UI failure\n"));
        persisted = true;
        return absl::OkStatus();
      });
  if (unavailable.code() != absl::StatusCode::kUnavailable || !persisted) {
    return absl::InternalError("UI failure interrupted persistence");
  }
  system_api::DebugLog("FA:UI_FAILURE_WRITE_DONE");

  Promise<Unit> opened;
  Promise<Unit> closed;
  const absl::Status result = system_api::internal::RunFailureReportTasks(
      [&] {
        const absl::Status shown = system_api::internal::RunFailureView(
            [&] { return Present(&opened); });
        if (!shown.ok()) {
          opened.SetError(shown);
        }
        // Present's window and the UI thread's native owners are already gone.
        closed.SetValue(Unit{});
        return shown;
      },
      [&]() -> absl::Status {
        ABSL_RETURN_IF_ERROR(opened.future().Await().status());
        system_api::DebugLog("FA:WRITER_WAITING_FOR_UI_EXIT");
        ABSL_RETURN_IF_ERROR(closed.future().Await().status());
        system_api::DebugLog("FA:WRITER_DRAINING_WITHOUT_WINDOW");
        thread::SleepFor(absl::Seconds(3));
        ABSL_RETURN_IF_ERROR(Save(kExpected));
        system_api::DebugLog("FA:WRITE_DONE");
        return absl::OkStatus();
      });
  return result;
}

}  // namespace

int main() {
  const absl::Status result = Run();
  if (!result.ok()) {
    system_api::DebugLog(result.ToString());
    return 2;
  }
  system_api::DebugLog("FA:ALL_TASKS_DONE");
  return 0;
}
