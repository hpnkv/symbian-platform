// SPDX-License-Identifier: Apache-2.0
#include "mbedtls/entropy.h"

int mbedtls_hardware_poll(void* data, unsigned char* output, size_t len,
                          size_t* olen) {
  (void)data;
  (void)output;
  (void)len;
  if (olen != NULL) {
    *olen = 0;
  }
  // No linkable, verified EABI source currently supplies secure guest entropy.
  return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
}
