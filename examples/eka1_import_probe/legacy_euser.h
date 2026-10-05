// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.
#ifndef SYMBIAN_EXAMPLES_EKA1_IMPORT_LEGACY_EUSER_H_
#define SYMBIAN_EXAMPLES_EKA1_IMPORT_LEGACY_EUSER_H_
// Explicit boundary: only the exercised integer/pointer signatures are
// modelled. Modern SDK C++ names/classes must not enter the EKA1 import ABI.
extern "C" void* LegacyAlloc(int size) asm("Alloc__4Useri");
extern "C" void LegacyFree(void* data) asm("Free__4UserPv");
extern "C" int LegacyAllocLen(const void* data) asm("AllocLen__4UserPCv");
extern "C" int LegacyAllocSize(int& total) asm("AllocSize__4UserRi");
extern "C" void* LegacyCopy(void* dest, const void* source,
                            int size) asm("Copy__3MemPvPCvi");
#endif
