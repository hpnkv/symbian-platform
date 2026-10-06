// SPDX-License-Identifier: Apache-2.0
#ifndef SYMBIAN_TLS_HEAP_H_
#define SYMBIAN_TLS_HEAP_H_
#include <absl/base/nullability.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
// Native Symbian heap adapters. All allocation and release must occur on the
// owning OS thread; failure returns NULL. These are not PIPS libc entry points.
void* absl_nullable symbian_tls_malloc(size_t size);
void* absl_nullable symbian_tls_calloc(size_t count, size_t size);
void* absl_nullable symbian_tls_realloc(void* absl_nullable pointer,
                                        size_t size);
void symbian_tls_free(void* absl_nullable pointer);
#ifdef __cplusplus
}
#endif
#endif
