// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include <cstddef>
#include <cstring>
#include <limits>

#include <absl/base/nullability.h>

#include "abi.h"

namespace {

bool FitsGuestAllocator(std::size_t size) {
  return size <= std::numeric_limits<unsigned int>::max();
}

unsigned int GuestAllocationSize(std::size_t size) {
  const std::size_t minimum = alignof(std::max_align_t);
  return static_cast<unsigned int>(size < minimum ? minimum : size);
}

}  // namespace

extern "C" void* absl_nullable malloc(std::size_t size) {
  if (!FitsGuestAllocator(size)) {
    return nullptr;
  }
  return SymbianRuntimeAllocate(GuestAllocationSize(size));
}

extern "C" void free(void* absl_nullable pointer) {
  SymbianRuntimeFree(pointer);
}

extern "C" void* absl_nullable calloc(std::size_t count, std::size_t size) {
  if (count != 0 && size > std::numeric_limits<std::size_t>::max() / count) {
    return nullptr;
  }
  const std::size_t bytes = count * size;
  void* absl_nullable pointer = malloc(bytes);
  if (pointer != nullptr) {
    std::memset(pointer, 0, bytes);
  }
  return pointer;
}

extern "C" void* absl_nullable realloc(void* absl_nullable pointer,
                                        std::size_t size) {
  if (!FitsGuestAllocator(size)) {
    return nullptr;
  }
  return SymbianRuntimeReallocate(
      pointer, size == 0 ? 0 : GuestAllocationSize(size));
}
