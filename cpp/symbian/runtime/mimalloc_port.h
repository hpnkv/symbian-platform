// Copyright 2026 Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.
// Target-only configuration for the pinned MIT-licensed mimalloc sources.
#ifndef SYMBIAN_RUNTIME_MIMALLOC_PORT_H_
#define SYMBIAN_RUNTIME_MIMALLOC_PORT_H_

#include <e32def.h>
#include <stdint.h>

#define MI_TLS_MODEL_PTHREADS 1
// Cache only mimalloc's own default/cached theap keys. The native bridge
// mirrors every write to pthread TLS so a miss retains the original contract.
#define pthread_getspecific SymbianMimallocGetSpecific
#define pthread_setspecific SymbianMimallocSetSpecific
extern uintptr_t SymbianRuntimeMimallocThreadId(void);
#define MI_PRIM_THREAD_ID() SymbianRuntimeMimallocThreadId()

#endif  // SYMBIAN_RUNTIME_MIMALLOC_PORT_H_
