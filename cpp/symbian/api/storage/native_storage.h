// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_API_STORAGE_NATIVE_STORAGE_H_
#define SYMBIAN_API_STORAGE_NATIVE_STORAGE_H_

#include <cstdint>

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

extern "C" int SymbianDeviceFileOpen(const char16_t* path, int length,
                                     NativeFile** output);
extern "C" int SymbianDeviceWritableFileOpen(const char16_t* path, int length,
                                             int mode, NativeFile** output);
extern "C" int SymbianDeviceFileWriteAt(NativeFile* file, int offset,
                                        const unsigned char* source,
                                        int length);
extern "C" int SymbianDeviceFileFlush(NativeFile* file);
extern "C" int SymbianDeviceCreateDirectories(const char16_t* path, int length);
extern "C" int SymbianDeviceFileSize(NativeFile* file, int* size);
extern "C" int SymbianDeviceFileReadAt(NativeFile* file, int offset,
                                       unsigned char* output, int capacity,
                                       int* bytes_read);
extern "C" void SymbianDeviceFileClose(NativeFile* file);
extern "C" int SymbianDeviceDirectoryOpen(const char16_t* path, int length,
                                          NativeDirectory** output);
extern "C" int SymbianDeviceDirectoryNext(NativeDirectory* directory,
                                          NativeDirectoryEntry* output,
                                          bool* end);
extern "C" void SymbianDeviceDirectoryClose(NativeDirectory* directory);

}  // namespace symbian::api::storage

#endif  // SYMBIAN_API_STORAGE_NATIVE_STORAGE_H_
