// SPDX-License-Identifier: Apache-2.0
#include "mbedtls/platform_util.h"

// Volatile writes remain observable even with optimization and link-time DCE.
// Separate from the inherited port's plain memset implementation.
void mbedtls_platform_zeroize(void* buffer, size_t size) {
  volatile unsigned char* bytes = (volatile unsigned char*)buffer;
  while (size-- != 0) {
    *bytes++ = 0;
  }
}
