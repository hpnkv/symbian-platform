# Storage component

**Implemented:** `Symbian::Storage` exports `ReadOnlyFile`, `WritableFile`,
`CreateDirectories` and `DirectoryReader` in
`<symbian/api/storage/storage.h>`.

## Motivation and modernization

The File Server APIs use `RFs` plus `RFile` or `RDir`, descriptor parameters,
integer errors and explicit `Close()` calls. Each public C++ object now owns
its session and subsession, is move-only, and closes both exactly once. A
`std::u16string_view` path preserves the native UTF-16 spelling without a
lossy conversion. `absl::StatusOr` distinguishes an empty file or directory
from an error, including `PermissionDenied` for protected paths.

## Ownership and cost

`ReadAt` fills a caller-owned `std::span<std::byte>` directly. Reusing one
open file avoids reconnecting to the File Server and allocating a buffer on
every read. `DirectoryReader::Next()` yields one owned entry at a time, so a
large directory never becomes a large result vector. The legacy handles and
descriptors remain in `native_storage.cc`. Paths must be absolute UTF-16 drive
paths. File Server calls may block; create, use and destroy each owner on the
same worker thread while event callbacks remain short. Native session owners must remain on the creating worker thread.

`WritableFile` uses the same ownership rule and reads directly from a caller
`std::span<const std::byte>` during synchronous `WriteAt`; there is no
per-write vector or string allocation. `WriteMode` distinguishes creating a
new file, opening an existing file, and deliberately replacing a file. Parent
creation is an explicit `CreateDirectories` call. `Flush` exposes the File
Server's flush request and its error without claiming storage hardware
durability beyond that native contract. The open file uses exclusive sharing,
so concurrent writers cannot silently race through this owner. All operations
respect the File Server's data-cage and drive permissions.

`FileCopy` is pull-driven rather than a long opaque call. It owns a reusable
32 KiB buffer and at most one read and one write per `Step()`. Callers can
publish each `CopyProgress` immediately and decide whether to schedule another
step. `Cancel()` is an atomic request callable from another thread; the next
checkpoint returns `Cancelled`, with partial progress still inspectable on
the worker. Directory listings return one entry per `Next()`; their own
`Cancel()` stops the next entry at a checkpoint. In-flight synchronous File
Server calls cannot yet be preempted with a proven wall-clock bound; these
APIs promise a bounded amount of work and memory per step, not a fixed maximum
I/O latency.
This distinction matters when an underlying drive or server stops responding.

## Read only a bounded prefix

For a preview or file signature, reuse caller storage instead of allocating the
whole file. `ReadAt` can return fewer bytes than requested:

```cpp
#include <array>
#include <string>

#include "symbian/api/storage/storage.h"

absl::StatusOr<std::string> ReadPreview(std::u16string_view path) {
  auto file = symbian::api::storage::ReadOnlyFile::Open(path);
  if (!file.ok()) {
    return file.status();
  }
  std::array<std::byte, 4096> buffer{};
  auto count = file->ReadAt(0, buffer);
  if (!count.ok()) {
    return count.status();
  }
  return std::string(reinterpret_cast<const char*>(buffer.data()), *count);
}
```

The returned bytes are not implicitly decoded as text. A text preview must
select an encoding and handle an incomplete final character.

## Save a small draft

Link `Symbian::Storage` and run the whole function on one worker. Substitute
your application's secure ID for `E0000830` in the private path. This explicitly
replaces the previous draft; it does not implement an atomic multi-file update.

```cpp
#include <span>
#include <string_view>

#include "symbian/api/storage/storage.h"

absl::Status SaveDraft(std::string_view utf8) {
  namespace files = symbian::api::storage;
  if (utf8.size() > 32768) {
    return absl::ResourceExhaustedError("Draft exceeds 32 KiB");
  }
  auto status = files::CreateDirectories(u"C:\\private\\E0000830\\");
  if (!status.ok()) {
    return status;
  }
  auto file = files::WritableFile::Open(u"C:\\private\\E0000830\\draft.txt",
                                        files::WriteMode::kReplaceExisting);
  if (!file.ok()) {
    return file.status();
  }
  status = file->WriteAt(0, std::as_bytes(std::span(utf8.data(), utf8.size())));
  return status.ok() ? file->Flush() : status;
}
```

The file and session close on return, including error paths. `Flush` reports the
File Server result; it cannot promise durability beyond the native service.

## Restrictions

Offsets must be below 2 GiB. Large-file APIs and metadata/watch subscriptions
are unavailable. Cancellation is checked between synchronous operations;
in-flight I/O has no fixed cancellation or timeout bound.
`WriteAt` does not provide an atomic update across multiple calls or rollback
after a failed write.
