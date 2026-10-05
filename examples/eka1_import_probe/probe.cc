// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.
#include "legacy_euser.h"
#ifndef SYMBIAN_EKA1_RESULT
#define SYMBIAN_EKA1_RESULT 7610
#endif
#ifndef SYMBIAN_EKA1_CORRUPT_COPY
#define SYMBIAN_EKA1_CORRUPT_COPY 0
#endif
extern "C" int Eka1Main() {
  int before_bytes = 0;
  const int before_cells = LegacyAllocSize(before_bytes);
  auto* data = static_cast<unsigned char*>(LegacyAlloc(64));
  if (data == nullptr) {
    return -4;
  }
  int result = 0;
  unsigned char source[64];
  for (int i = 0; i < 64; ++i) {
    source[i] = static_cast<unsigned char>(i ^ 0x5a);
  }
  const bool valid_length = LegacyAllocLen(data) >= 64;
  void* end = LegacyCopy(data, source, 64);
  if (!valid_length || end != data + 64) {
    result = 40;
  }
  if (SYMBIAN_EKA1_CORRUPT_COPY) {
    data[17] ^= 1;
  }
  for (int i = 0; i < 64; ++i) {
    if (data[i] != source[i]) {
      result = 41;
    }
  }
  int live_bytes = 0;
  if (LegacyAllocSize(live_bytes) != before_cells + 1 ||
      live_bytes < before_bytes + 64) {
    result = 42;
  }
  LegacyFree(data);
  int after_bytes = 0;
  if (LegacyAllocSize(after_bytes) != before_cells ||
      after_bytes != before_bytes) {
    result = 43;
  }
  return result == 0 ? SYMBIAN_EKA1_RESULT : result;
}
