// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "native_storage.h"

#include <absl/base/nullability.h>
#include <e32std.h>
#include <f32file.h>

namespace symbian::api::storage {

struct NativeFile {
  RFs session;
  RFile file;
  bool file_open = false;
};

struct NativeDirectory {
  RFs session;
  RDir directory;
  bool directory_open = false;
};

extern "C" void SymbianDeviceFileClose(NativeFile* absl_nullable file) {
  if (file == nullptr) {
    return;
  }
  if (file->file_open) {
    file->file.Close();
  }
  file->session.Close();
  file->~NativeFile();
  User::Free(file);
}

extern "C" int SymbianDeviceFileOpen(
    const char16_t* absl_nullable path, int length,
    NativeFile* absl_nullable* absl_nullable output) {
  if (output == nullptr || path == nullptr || length <= 0 ||
      length > KMaxFileName) {
    return KErrArgument;
  }
  *output = nullptr;
  void* absl_nullable memory = User::Alloc(sizeof(NativeFile));
  if (memory == nullptr) {
    return KErrNoMemory;
  }
  auto* absl_nonnull file = new (memory) NativeFile;
  const TInt connected = file->session.Connect();
  if (connected != KErrNone) {
    file->~NativeFile();
    User::Free(file);
    return connected;
  }
  const TPtrC16 native_path(reinterpret_cast<const TText* absl_nonnull>(path),
                            length);
  const TInt opened = file->file.Open(file->session, native_path,
                                      EFileRead | EFileShareReadersOnly);
  if (opened != KErrNone) {
    SymbianDeviceFileClose(file);
    return opened;
  }
  file->file_open = true;
  *output = file;
  return KErrNone;
}

extern "C" int SymbianDeviceWritableFileOpen(
    const char16_t* absl_nullable path, int length, int mode,
    NativeFile* absl_nullable* absl_nullable output) {
  if (output == nullptr || path == nullptr || length <= 0 ||
      length > KMaxFileName || mode < 0 || mode > 2) {
    return KErrArgument;
  }
  *output = nullptr;
  void* absl_nullable memory = User::Alloc(sizeof(NativeFile));
  if (memory == nullptr) {
    return KErrNoMemory;
  }
  auto* absl_nonnull file = new (memory) NativeFile;
  const TInt connected = file->session.Connect();
  if (connected != KErrNone) {
    file->~NativeFile();
    User::Free(file);
    return connected;
  }
  const TPtrC16 native_path(reinterpret_cast<const TText* absl_nonnull>(path),
                            length);
  const TUint flags = EFileRead | EFileWrite | EFileShareExclusive;
  TInt opened = KErrArgument;
  switch (mode) {
    case 0:
      opened = file->file.Create(file->session, native_path, flags);
      break;
    case 1:
      opened = file->file.Open(file->session, native_path, flags);
      break;
    case 2:
      opened = file->file.Replace(file->session, native_path, flags);
      break;
  }
  if (opened != KErrNone) {
    SymbianDeviceFileClose(file);
    return opened;
  }
  file->file_open = true;
  *output = file;
  return KErrNone;
}

extern "C" int SymbianDeviceFileWriteAt(
    NativeFile* absl_nullable file, int offset,
    const unsigned char* absl_nullable source, int length) {
  if (file == nullptr || source == nullptr || offset < 0 || length < 0) {
    return KErrArgument;
  }
  const TPtrC8 bytes(source, length);
  return file->file.Write(offset, bytes);
}

extern "C" int SymbianDeviceFileFlush(NativeFile* absl_nullable file) {
  if (file == nullptr) {
    return KErrArgument;
  }
  return file->file.Flush();
}

extern "C" int SymbianDeviceCreateDirectories(
    const char16_t* absl_nullable path, int length) {
  if (path == nullptr || length <= 0 || length > KMaxFileName) {
    return KErrArgument;
  }
  const bool ends_in_separator = path[length - 1] == u'\\';
  if (!ends_in_separator && length == KMaxFileName) {
    return KErrArgument;
  }
  RFs session;
  const TInt connected = session.Connect();
  if (connected != KErrNone) {
    return connected;
  }
  const TPtrC16 native_path(reinterpret_cast<const TText* absl_nonnull>(path),
                            length);
  TBuf<KMaxFileName> directory;
  directory.Copy(native_path);
  if (!ends_in_separator) {
    directory.Append(u'\\');
  }
  const TInt created = session.MkDirAll(directory);
  session.Close();
  return created == KErrAlreadyExists ? KErrNone : created;
}

extern "C" int SymbianDeviceFileSize(NativeFile* absl_nullable file,
                                     int* absl_nullable size) {
  if (file == nullptr || size == nullptr) {
    return KErrArgument;
  }
  return file->file.Size(*size);
}

extern "C" int SymbianDeviceFileReadAt(NativeFile* absl_nullable file,
                                       int offset,
                                       unsigned char* absl_nullable output,
                                       int capacity,
                                       int* absl_nullable bytes_read) {
  if (file == nullptr || output == nullptr || bytes_read == nullptr ||
      offset < 0 || capacity < 0) {
    return KErrArgument;
  }
  *bytes_read = 0;
  TPtr8 destination(output, 0, capacity);
  const TInt result = file->file.Read(offset, destination);
  if (result == KErrNone) {
    *bytes_read = destination.Length();
  }
  return result;
}

extern "C" void SymbianDeviceDirectoryClose(
    NativeDirectory* absl_nullable directory) {
  if (directory == nullptr) {
    return;
  }
  if (directory->directory_open) {
    directory->directory.Close();
  }
  directory->session.Close();
  directory->~NativeDirectory();
  User::Free(directory);
}

extern "C" int SymbianDeviceDirectoryOpen(
    const char16_t* absl_nullable path, int length,
    NativeDirectory* absl_nullable* absl_nullable output) {
  if (output == nullptr || path == nullptr || length <= 0 || length > 253) {
    return KErrArgument;
  }
  *output = nullptr;
  void* absl_nullable memory = User::Alloc(sizeof(NativeDirectory));
  if (memory == nullptr) {
    return KErrNoMemory;
  }
  auto* absl_nonnull directory = new (memory) NativeDirectory;
  const TInt connected = directory->session.Connect();
  if (connected != KErrNone) {
    directory->~NativeDirectory();
    User::Free(directory);
    return connected;
  }
  TText pattern[256];
  for (int index = 0; index < length; ++index) {
    pattern[index] = path[index];
  }
  int pattern_length = length;
  if (pattern[pattern_length - 1] != '\\') {
    pattern[pattern_length++] = '\\';
  }
  pattern[pattern_length++] = '*';
  const TPtrC16 native_pattern(pattern, pattern_length);
  const TInt opened = directory->directory.Open(
      directory->session, native_pattern,
      KEntryAttDir | KEntryAttHidden | KEntryAttSystem);
  if (opened != KErrNone) {
    SymbianDeviceDirectoryClose(directory);
    return opened;
  }
  directory->directory_open = true;
  *output = directory;
  return KErrNone;
}

extern "C" int SymbianDeviceDirectoryNext(
    NativeDirectory* absl_nullable directory,
    NativeDirectoryEntry* absl_nullable output, bool* absl_nullable end) {
  if (directory == nullptr || output == nullptr || end == nullptr) {
    return KErrArgument;
  }
  *end = false;
  TEntry entry;
  const TInt result = directory->directory.Read(entry);
  if (result == KErrEof) {
    *end = true;
    return KErrNone;
  }
  if (result != KErrNone) {
    return result;
  }
  const TInt length = entry.iName.Length();
  if (length < 0 || length > 256) {
    return KErrCorrupt;
  }
  const TText* absl_nonnull name = entry.iName.Ptr();
  for (TInt index = 0; index < length; ++index) {
    output->name[index] = name[index];
  }
  output->name_length = length;
  output->is_directory = (entry.iAtt & KEntryAttDir) != 0;
  output->is_read_only = (entry.iAtt & KEntryAttReadOnly) != 0;
  output->size_bytes = output->is_directory ? 0 : entry.FileSize();
  return KErrNone;
}

}  // namespace symbian::api::storage
