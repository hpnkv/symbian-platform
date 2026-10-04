// SPDX-License-Identifier: Apache-2.0
#include "symbian_tls/heap.h"
#include <string.h>

extern void *SymbianRuntimeAllocate(size_t size);
extern void SymbianRuntimeFree(void *pointer);

// Two size_t words preserve the SDK heap's 8-byte guest alignment. The prefix
// records the extent required by realloc; only pointers from this adapter belong
// here. The platform API takes a signed 32-bit length, including the prefix.
typedef struct {
  size_t size;
  size_t reserved;
} Allocation;

void *symbian_tls_malloc(size_t size) {
  if (size > 0x7fffffffu - sizeof(Allocation)) {
    return NULL;
  }
  Allocation *allocation = SymbianRuntimeAllocate(sizeof(Allocation) +
                                                 (size == 0 ? 1 : size));
  if (allocation == NULL) {
    return NULL;
  }
  allocation->size = size;
  allocation->reserved = 0;
  return allocation + 1;
}

void symbian_tls_free(void *pointer) {
  if (pointer != NULL) {
    SymbianRuntimeFree((Allocation *)pointer - 1);
  }
}

void *symbian_tls_calloc(size_t count, size_t size) {
  if (size != 0 && count > (0x7fffffffu - sizeof(Allocation)) / size) {
    return NULL;
  }
  size_t total = count * size;
  void *result = symbian_tls_malloc(total);
  if (result != NULL) {
    memset(result, 0, total);
  }
  return result;
}

void *symbian_tls_realloc(void *pointer, size_t size) {
  if (pointer == NULL) {
    return symbian_tls_malloc(size);
  }
  if (size == 0) {
    symbian_tls_free(pointer);
    return NULL;
  }
  void *result = symbian_tls_malloc(size);
  if (result == NULL) {
    return NULL;
  }
  size_t original_size = ((Allocation *)pointer - 1)->size;
  memcpy(result, pointer, original_size < size ? original_size : size);
  symbian_tls_free(pointer);
  return result;
}
