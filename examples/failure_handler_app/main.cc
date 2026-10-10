// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/status_macros.h"
#include "absl/strings/str_format.h"
#include "symbian/api/display/window_surface.h"

namespace {

absl::Status Main() {
  ABSL_ASSIGN_OR_RETURN(
      auto window,
      symbian::api::display::WindowSurface::Create("Failure handler demo"));
  ABSL_RETURN_IF_ERROR(window.SetAutomaticOrientation(true));

  LOG(INFO) << "Short log line.";
  LOG(WARNING) << "Long log line: this deliberately exceeds the display width "
                  "so the failure report must wrap it using the device font. "
                  "Drag the report up and down: the text should follow the "
                  "pointer without jumping by rows, and the Copy Logs and Exit "
                  "buttons should remain visible throughout the gesture.";
  LOG(INFO) << absl::StrFormat(
      "Formatted values: count=%04d, ratio=%.2f, hex=0x%X", 7, 0.625, 0xbeef);
  LOG(INFO) << "Multiple lines:\n  An indented line.\n\tA tabbed line.";
  LOG(INFO) << "Unicode text: caf\u00e9, \u03bb, \u2603.";
  for (int index = 0; index < 4; ++index) {
    LOG_FIRST_N(INFO, 2) << "Rate-limited log " << index;
  }
  LOG(ERROR) << "The next CHECK fails intentionally; this is the example's "
                "expected result. Use Copy Logs to copy the complete report.";
  CHECK_EQ(2 + 2, 5) << "Intentional failure-handler example";
  return absl::OkStatus();
}

}  // namespace

int main() {
  CHECK_OK(Main());
  return 0;
}
