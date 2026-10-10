// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "symbian/api/storage/storage.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <limits>
#include <new>
#include <utility>

#include <absl/status/status_macros.h>
#include <absl/base/nullability.h>

#include "native_storage.h"
#include "symbian/native_status.h"

namespace symbian::api::storage {
namespace {

absl::Status ValidatePath(std::u16string_view path,
                          std::size_t maximum_length) {
  if (path.size() < 3 || path.size() > maximum_length || path[1] != u':' ||
      path[2] != u'\\' ||
      !((path[0] >= u'A' && path[0] <= u'Z') ||
        (path[0] >= u'a' && path[0] <= u'z')) ||
      std::find(path.begin(), path.end(), u'\0') != path.end()) {
    return absl::InvalidArgumentError(
        "Expected an absolute native UTF-16 drive path");
  }
  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<ReadOnlyFile> ReadOnlyFile::Open(std::u16string_view path) {
  ABSL_RETURN_IF_ERROR(ValidatePath(path, 255));
  NativeFile* absl_nullable native = nullptr;
  if (const int result = SymbianDeviceFileOpen(
          path.data(), static_cast<int>(path.size()), &native);
      result != 0) {
    return symbian::StatusFromNativeError(result, "Open read-only file");
  }
  return ReadOnlyFile(native);
}

ReadOnlyFile::ReadOnlyFile(ReadOnlyFile&& other) noexcept
    : native_(std::exchange(other.native_, nullptr)) {}

ReadOnlyFile& ReadOnlyFile::operator=(ReadOnlyFile&& other) noexcept {
  if (this != &other) {
    SymbianDeviceFileClose(native_);
    native_ = std::exchange(other.native_, nullptr);
  }
  return *this;
}

ReadOnlyFile::~ReadOnlyFile() {
  SymbianDeviceFileClose(native_);
}

absl::StatusOr<WritableFile> WritableFile::Open(std::u16string_view path,
                                                WriteMode mode) {
  ABSL_RETURN_IF_ERROR(ValidatePath(path, 255));
  if (mode != WriteMode::kCreateNew && mode != WriteMode::kOpenExisting &&
      mode != WriteMode::kReplaceExisting) {
    return absl::InvalidArgumentError("Unknown file write mode");
  }
  NativeFile* absl_nullable native = nullptr;
  if (const int result = SymbianDeviceWritableFileOpen(
          path.data(), static_cast<int>(path.size()), static_cast<int>(mode),
          &native);
      result != 0) {
    return symbian::StatusFromNativeError(result, "Open writable file");
  }
  return WritableFile(native);
}

WritableFile::WritableFile(WritableFile&& other) noexcept
    : native_(std::exchange(other.native_, nullptr)) {}

WritableFile& WritableFile::operator=(WritableFile&& other) noexcept {
  if (this != &other) {
    SymbianDeviceFileClose(native_);
    native_ = std::exchange(other.native_, nullptr);
  }
  return *this;
}

WritableFile::~WritableFile() {
  SymbianDeviceFileClose(native_);
}

absl::Status WritableFile::WriteAt(std::uint64_t offset,
                                   std::span<const std::byte> source) {
  if (native_ == nullptr) {
    return absl::FailedPreconditionError("File is closed");
  }
  if (constexpr auto kMaximumOffset =
          static_cast<std::uint64_t>(std::numeric_limits<int>::max());
      offset > kMaximumOffset ||
      source.size() > static_cast<std::size_t>(kMaximumOffset - offset)) {
    return absl::UnimplementedError("Write exceeds 2 GiB profile");
  }
  if (source.empty()) {
    return absl::OkStatus();
  }
  const int result = SymbianDeviceFileWriteAt(
      native_, static_cast<int>(offset),
      reinterpret_cast<const unsigned char*>(source.data()),
      static_cast<int>(source.size()));
  return result == 0 ? absl::OkStatus()
                     : symbian::StatusFromNativeError(result, "Write file");
}

absl::Status WritableFile::Flush() {
  if (native_ == nullptr) {
    return absl::FailedPreconditionError("File is closed");
  }
  const int result = SymbianDeviceFileFlush(native_);
  return result == 0 ? absl::OkStatus()
                     : symbian::StatusFromNativeError(result, "Flush file");
}

absl::Status CreateDirectories(std::u16string_view path) {
  ABSL_RETURN_IF_ERROR(ValidatePath(path, 255));
  const int result = SymbianDeviceCreateDirectories(
      path.data(), static_cast<int>(path.size()));
  return result == 0
             ? absl::OkStatus()
             : symbian::StatusFromNativeError(result, "Create directories");
}

struct FileCopy::State {
  State(ReadOnlyFile source_file, WritableFile destination_file,
        std::uint64_t source_size)
      : source(std::move(source_file)),
        destination(std::move(destination_file)),
        total_bytes(source_size) {}

  ReadOnlyFile source;
  WritableFile destination;
  std::array<std::byte, 32 * 1024> buffer{};
  std::atomic<bool> cancel_requested{false};
  std::uint64_t bytes_copied = 0;
  std::uint64_t total_bytes = 0;
  bool complete = false;
};

FileCopy::FileCopy(State* absl_nonnull state) : state_(state) {}

absl::StatusOr<FileCopy> FileCopy::Open(std::u16string_view source,
                                        std::u16string_view destination,
                                        WriteMode destination_mode) {
  ABSL_ASSIGN_OR_RETURN(auto opened_source, ReadOnlyFile::Open(source));
  ABSL_ASSIGN_OR_RETURN(auto size, opened_source.Size());
  // Reserve memory before a mode that can create or replace the destination
  // has any filesystem side effect.
  void* absl_nullable memory = ::operator new(sizeof(State), std::nothrow);
  if (memory == nullptr) {
    return absl::ResourceExhaustedError("Allocate file copy buffer");
  }
  auto opened_destination = WritableFile::Open(destination, destination_mode);
  if (!opened_destination.ok()) {
    ::operator delete(memory);
    return opened_destination.status();
  }
  State* absl_nonnull state = new (memory)
      State(std::move(opened_source), std::move(*opened_destination), size);
  return FileCopy(state);
}

FileCopy::FileCopy(FileCopy&& other) noexcept
    : state_(std::exchange(other.state_, nullptr)) {}

FileCopy& FileCopy::operator=(FileCopy&& other) noexcept {
  if (this != &other) {
    delete state_;
    state_ = std::exchange(other.state_, nullptr);
  }
  return *this;
}

FileCopy::~FileCopy() {
  delete state_;
}

void FileCopy::Cancel() noexcept {
  if (state_ != nullptr) {
    state_->cancel_requested.store(true, std::memory_order_release);
  }
}

CopyProgress FileCopy::progress() const noexcept {
  if (state_ == nullptr) {
    return {};
  }
  return {.bytes_copied = state_->bytes_copied,
          .total_bytes = state_->total_bytes,
          .complete = state_->complete};
}

absl::StatusOr<CopyProgress> FileCopy::Step() {
  if (state_ == nullptr) {
    return absl::FailedPreconditionError("File copy is closed");
  }
  if (state_->complete) {
    return progress();
  }
  if (state_->cancel_requested.load(std::memory_order_acquire)) {
    return absl::CancelledError("File copy cancelled");
  }
  if (state_->bytes_copied < state_->total_bytes) {
    const auto remaining = state_->total_bytes - state_->bytes_copied;
    const auto capacity = static_cast<std::size_t>(
        std::min<std::uint64_t>(remaining, state_->buffer.size()));
    ABSL_ASSIGN_OR_RETURN(auto read, state_->source.ReadAt(
        state_->bytes_copied,
        std::span<std::byte>(state_->buffer.data(), capacity)));
    if (read == 0) {
      return absl::DataLossError("Source ended during file copy");
    }
    if (state_->cancel_requested.load(std::memory_order_acquire)) {
      return absl::CancelledError("File copy cancelled");
    }
    ABSL_RETURN_IF_ERROR(state_->destination.WriteAt(
            state_->bytes_copied,
            std::span<const std::byte>(state_->buffer.data(), read)));
    state_->bytes_copied += read;
  }
  if (state_->cancel_requested.load(std::memory_order_acquire)) {
    return absl::CancelledError("File copy cancelled");
  }
  if (state_->bytes_copied == state_->total_bytes) {
    ABSL_RETURN_IF_ERROR(state_->destination.Flush());
    state_->complete = true;
  }
  return progress();
}

absl::StatusOr<std::uint64_t> ReadOnlyFile::Size() const {
  if (native_ == nullptr) {
    return absl::FailedPreconditionError("File is closed");
  }
  int size = 0;
  if (const int result = SymbianDeviceFileSize(native_, &size); result != 0) {
    return symbian::StatusFromNativeError(result, "Read file size");
  }
  if (size < 0) {
    return absl::UnimplementedError("File exceeds 2 GiB profile");
  }
  return static_cast<std::uint64_t>(size);
}

absl::StatusOr<std::size_t> ReadOnlyFile::ReadAt(
    std::uint64_t offset, std::span<std::byte> destination) const {
  if (native_ == nullptr) {
    return absl::FailedPreconditionError("File is closed");
  }
  if (constexpr auto kMaximumOffset =
          static_cast<std::uint64_t>(std::numeric_limits<int>::max());
      offset > kMaximumOffset ||
      destination.size() > static_cast<std::size_t>(kMaximumOffset - offset)) {
    return absl::UnimplementedError("Read exceeds 2 GiB profile");
  }
  if (destination.empty()) {
    return std::size_t{0};
  }
  int bytes_read = 0;
  if (const int result = SymbianDeviceFileReadAt(
          native_, static_cast<int>(offset),
          reinterpret_cast<unsigned char*>(destination.data()),
          static_cast<int>(destination.size()), &bytes_read);
      result != 0) {
    return symbian::StatusFromNativeError(result, "Read file");
  }
  if (bytes_read < 0 ||
      static_cast<std::size_t>(bytes_read) > destination.size()) {
    return absl::DataLossError("File Server returned invalid read length");
  }
  return static_cast<std::size_t>(bytes_read);
}

absl::StatusOr<DirectoryReader> DirectoryReader::Open(
    std::u16string_view path) {
  ABSL_RETURN_IF_ERROR(ValidatePath(path, 253));
  NativeDirectory* absl_nullable native = nullptr;
  if (const int result = SymbianDeviceDirectoryOpen(
          path.data(), static_cast<int>(path.size()), &native);
      result != 0) {
    return symbian::StatusFromNativeError(result, "Open directory");
  }
  return DirectoryReader(native);
}

DirectoryReader::DirectoryReader(DirectoryReader&& other) noexcept
    : native_(std::exchange(other.native_, nullptr)),
      cancel_requested_(
          other.cancel_requested_.load(std::memory_order_relaxed)) {}

DirectoryReader& DirectoryReader::operator=(DirectoryReader&& other) noexcept {
  if (this != &other) {
    SymbianDeviceDirectoryClose(native_);
    native_ = std::exchange(other.native_, nullptr);
    cancel_requested_.store(
        other.cancel_requested_.load(std::memory_order_relaxed),
        std::memory_order_relaxed);
  }
  return *this;
}

DirectoryReader::~DirectoryReader() {
  SymbianDeviceDirectoryClose(native_);
}

void DirectoryReader::Cancel() noexcept {
  cancel_requested_.store(true, std::memory_order_release);
}

absl::StatusOr<std::optional<DirectoryEntry>> DirectoryReader::Next() {
  if (native_ == nullptr) {
    return absl::FailedPreconditionError("Directory is closed");
  }
  if (cancel_requested_.load(std::memory_order_acquire)) {
    return absl::CancelledError("Directory listing cancelled");
  }
  NativeDirectoryEntry native_entry;
  bool end = false;
  const int result = SymbianDeviceDirectoryNext(native_, &native_entry, &end);
  if (cancel_requested_.load(std::memory_order_acquire)) {
    return absl::CancelledError("Directory listing cancelled");
  }
  if (result != 0) {
    return symbian::StatusFromNativeError(result, "Read directory");
  }
  if (end) {
    return std::nullopt;
  }
  if (native_entry.name_length < 0 || native_entry.name_length > 256) {
    return absl::DataLossError("File Server returned invalid entry name");
  }
  DirectoryEntry entry;
  entry.name.assign(native_entry.name,
                    native_entry.name + native_entry.name_length);
  entry.is_directory = native_entry.is_directory;
  entry.is_read_only = native_entry.is_read_only;
  if (!entry.is_directory) {
    entry.size_bytes = native_entry.size_bytes;
  }
  return std::optional<DirectoryEntry>(std::move(entry));
}

}  // namespace symbian::api::storage
