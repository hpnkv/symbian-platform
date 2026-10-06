// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_API_STORAGE_NATIVE_STORAGE_H_
#define SYMBIAN_API_STORAGE_NATIVE_STORAGE_H_

#include <cstdint>

#include <absl/base/nullability.h>

namespace symbian::api::storage {

struct NativeFile;
struct NativeDirectory;

struct NativeDirectoryEntry {
  char16_t name[256]{};
  int name_length = 0;
  bool is_directory = false;
  bool is_read_only = false;
  std::uint64_t size_bytes = 0;
};

extern "C" int SymbianDeviceFileOpen(
    const char16_t* absl_nullable path, int length,
    NativeFile* absl_nullable* absl_nullable output);
extern "C" int SymbianDeviceWritableFileOpen(
    const char16_t* absl_nullable path, int length, int mode,
    NativeFile* absl_nullable* absl_nullable output);
extern "C" int SymbianDeviceFileWriteAt(
    NativeFile* absl_nullable file, int offset,
    const unsigned char* absl_nullable source, int length);
extern "C" int SymbianDeviceFileFlush(NativeFile* absl_nullable file);
extern "C" int SymbianDeviceCreateDirectories(
    const char16_t* absl_nullable path, int length);
extern "C" int SymbianDeviceFileSize(NativeFile* absl_nullable file,
                                     int* absl_nullable size);
extern "C" int SymbianDeviceFileReadAt(NativeFile* absl_nullable file,
                                       int offset,
                                       unsigned char* absl_nullable output,
                                       int capacity,
                                       int* absl_nullable bytes_read);
extern "C" void SymbianDeviceFileClose(NativeFile* absl_nullable file);
extern "C" int SymbianDeviceDirectoryOpen(
    const char16_t* absl_nullable path, int length,
    NativeDirectory* absl_nullable* absl_nullable output);
extern "C" int SymbianDeviceDirectoryNext(
    NativeDirectory* absl_nullable directory,
    NativeDirectoryEntry* absl_nullable output, bool* absl_nullable end);
extern "C" void SymbianDeviceDirectoryClose(
    NativeDirectory* absl_nullable directory);

}  // namespace symbian::api::storage

#endif  // SYMBIAN_API_STORAGE_NATIVE_STORAGE_H_
