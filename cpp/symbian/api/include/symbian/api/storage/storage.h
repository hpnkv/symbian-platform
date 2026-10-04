// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_API_STORAGE_STORAGE_H_
#define SYMBIAN_API_STORAGE_STORAGE_H_

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "absl/status/statusor.h"

namespace symbian::api::storage {

struct NativeFile;
struct NativeDirectory;

// One open file and its owning File Server session. Move-only and read-only.
// Keep use and destruction on the opening thread; native session ownership
// across threads is not yet established.
class ReadOnlyFile {
 public:
  // Opens an absolute Symbian UTF-16 path (for example u"C:\\Data\\file.txt").
  static absl::StatusOr<ReadOnlyFile> Open(std::u16string_view path);

  ReadOnlyFile(ReadOnlyFile&& other) noexcept;
  ReadOnlyFile& operator=(ReadOnlyFile&& other) noexcept;
  ReadOnlyFile(const ReadOnlyFile&) = delete;
  ReadOnlyFile& operator=(const ReadOnlyFile&) = delete;
  ~ReadOnlyFile();

  // Returns the file's size. This initial File Server profile supports 2 GiB.
  absl::StatusOr<std::uint64_t> Size() const;
  // Reads directly into caller memory, with no per-read allocation. An empty
  // destination returns zero. Offsets above 2 GiB are unsupported here.
  absl::StatusOr<std::size_t> ReadAt(std::uint64_t offset,
                                     std::span<std::byte> destination) const;

 private:
  explicit ReadOnlyFile(NativeFile* native) : native_(native) {}

  NativeFile* native_ = nullptr;
};

// Creation and replacement are explicit so opening a file cannot silently
// truncate existing data.
enum class WriteMode {
  kCreateNew,
  kOpenExisting,
  kReplaceExisting,
};

// One writable file and its File Server session. Move-only; use and destroy
// on the opening thread. Calls can block, so run them on a worker if needed.
class WritableFile {
 public:
  // Opens an absolute UTF-16 path. Parent directories must already exist.
  static absl::StatusOr<WritableFile> Open(std::u16string_view path,
                                           WriteMode mode);

  WritableFile(WritableFile&& other) noexcept;
  WritableFile& operator=(WritableFile&& other) noexcept;
  WritableFile(const WritableFile&) = delete;
  WritableFile& operator=(const WritableFile&) = delete;
  ~WritableFile();

  // Writes all bytes directly from caller memory at an explicit offset.
  // The caller keeps the buffer valid until the call returns.
  absl::Status WriteAt(std::uint64_t offset, std::span<const std::byte> source);
  // Requests that the File Server flush this file's pending writes.
  absl::Status Flush();

 private:
  explicit WritableFile(NativeFile* native) : native_(native) {}

  NativeFile* native_ = nullptr;
};

// Progress after one bounded-memory transfer step.
struct CopyProgress {
  // Number of bytes already written to the destination.
  std::uint64_t bytes_copied = 0;
  // Source size recorded when copying began.
  std::uint64_t total_bytes = 0;
  // True after the final write and File Server flush have succeeded.
  bool complete = false;
};

// Pull-driven file copy. Each Step performs at most one 32 KiB read and write;
// callers can publish each result, then schedule the next Step on the same
// worker. Cancel() is safe from another thread while the object stays alive.
// Native synchronous I/O already in flight may finish before Step returns.
class FileCopy {
 public:
  // Destination mode makes replacement explicit. Both paths must be absolute.
  static absl::StatusOr<FileCopy> Open(std::u16string_view source,
                                       std::u16string_view destination,
                                       WriteMode destination_mode);

  FileCopy(FileCopy&& other) noexcept;
  FileCopy& operator=(FileCopy&& other) noexcept;
  FileCopy(const FileCopy&) = delete;
  FileCopy& operator=(const FileCopy&) = delete;
  ~FileCopy();

  // Requests cancellation without waiting for File Server I/O. The next
  // cancellation checkpoint returns kCancelled. Do not destroy or move the
  // operation concurrently with Cancel or Step.
  void Cancel() noexcept;
  // Runs one transfer step. A completed call is idempotent. No concurrent Step
  // calls; all Steps and destruction stay on the opening thread.
  absl::StatusOr<CopyProgress> Step();
  // May be called from the worker between steps to recover partial progress.
  CopyProgress progress() const noexcept;

 private:
  struct State;
  explicit FileCopy(State* state);
  State* state_ = nullptr;
};

// Creates missing parents for an absolute UTF-16 directory path. The native
// File Server enforces drive and data-cage permissions.
absl::Status CreateDirectories(std::u16string_view path);

// A single directory entry; file size is meaningful only for files.
struct DirectoryEntry {
  // Name relative to the opened directory, in native UTF-16.
  std::u16string name;
  // Whether this entry is a directory.
  bool is_directory = false;
  // Native read-only attribute.
  bool is_read_only = false;
  // Size in bytes for files; empty for directories.
  std::optional<std::uint64_t> size_bytes;
};

// A streaming, move-only directory cursor with bounded memory use. Keep use
// and destruction on the opening thread.
class DirectoryReader {
 public:
  // Opens a UTF-16 directory path. Entries are returned one at a time.
  static absl::StatusOr<DirectoryReader> Open(std::u16string_view path);

  DirectoryReader(DirectoryReader&& other) noexcept;
  DirectoryReader& operator=(DirectoryReader&& other) noexcept;
  DirectoryReader(const DirectoryReader&) = delete;
  DirectoryReader& operator=(const DirectoryReader&) = delete;
  ~DirectoryReader();

  // Requests that iteration stop. This is safe from another thread while the
  // reader remains alive. An in-flight native read may finish before Next()
  // returns kCancelled. Do not move or destroy concurrently with Cancel().
  void Cancel() noexcept;

  // Returns an empty optional at end. A cursor is not safe for simultaneous
  // calls from multiple threads. File Server calls may block: use a worker.
  absl::StatusOr<std::optional<DirectoryEntry>> Next();

 private:
  explicit DirectoryReader(NativeDirectory* native) : native_(native) {}

  NativeDirectory* native_ = nullptr;
  std::atomic<bool> cancel_requested_{false};
};

}  // namespace symbian::api::storage

#endif  // SYMBIAN_API_STORAGE_STORAGE_H_
