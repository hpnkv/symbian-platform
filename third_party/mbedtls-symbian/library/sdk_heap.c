// SPDX-License-Identifier: Apache-2.0
#include <absl/base/nullability.h>
#include <string.h>

#include "symbian_mbedtls/platform.h"

// Existing C ABI from the SDK runtime. No modern C++/native SDK header mixing.
extern void* absl_nullable SymbianRuntimeAllocate(size_t size);
extern void SymbianRuntimeFree(void* absl_nullable pointer);

void* absl_nullable symbian_mbedtls_calloc(size_t count, size_t size) {
  const size_t maximum = 0x7fffffff;
  if (size != 0 && count > maximum / size) {
    return NULL;
  }
  size_t total = count * size;
  void* absl_nullable result = SymbianRuntimeAllocate(total == 0 ? 1 : total);
  if (result != NULL) {
    memset(result, 0, total);
  }
  return result;
}

void symbian_mbedtls_free(void* absl_nullable pointer) {
  SymbianRuntimeFree(pointer);
}
