// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "trace.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

#include "symbian/api/storage/storage.h"
#include "symbian/api/system/debug_log.h"

namespace camera_app {
namespace {

namespace storage = symbian::api::storage;

#if defined(SYMBIAN_CAMERA_GPU_PROBE)
constexpr std::u16string_view kTraceDirectory = u"E:\\Others\\";
constexpr std::u16string_view kTracePath =
    u"E:\\Others\\camera_app_gles2_trace.txt";
#else
constexpr std::u16string_view kTraceDirectory = u"E:\\Others\\";
constexpr std::u16string_view kTracePath = u"E:\\Others\\camera_app_trace.txt";
#endif

std::optional<storage::WritableFile> trace_file;
std::uint64_t trace_offset = 0;
absl::Status failure = absl::OkStatus();

}  // namespace

void OpenTrace() {
  trace_offset = 0;
  failure = absl::OkStatus();
  storage::CreateDirectories(kTraceDirectory).IgnoreError();
  if (auto opened = storage::WritableFile::Open(
          kTracePath, storage::WriteMode::kReplaceExisting);
      opened.ok()) {
    trace_file.emplace(std::move(*opened));
  }
}

void CloseTrace() {
  trace_file.reset();
}

bool TraceReady() {
  return trace_file.has_value();
}

void Trace(std::string_view message) {
  symbian::api::system::DebugLog(message);
  if (!trace_file.has_value()) {
    return;
  }
  const auto bytes = std::as_bytes(std::span(message.data(), message.size()));
  absl::Status written = trace_file->WriteAt(trace_offset, bytes);
  if (!written.ok()) {
    trace_file.reset();
    return;
  }
  trace_offset += bytes.size();
  constexpr char kNewline = '\n';
  written =
      trace_file->WriteAt(trace_offset, std::as_bytes(std::span(&kNewline, 1)));
  if (!written.ok()) {
    trace_file.reset();
    return;
  }
  ++trace_offset;
  trace_file->Flush().IgnoreError();
}

void RecordFailure(absl::Status error) {
  if (failure.ok()) {
    failure = std::move(error);
  }
  Trace(failure.ToString());
}

absl::Status FailureStatus() {
  return failure;
}

}  // namespace camera_app
