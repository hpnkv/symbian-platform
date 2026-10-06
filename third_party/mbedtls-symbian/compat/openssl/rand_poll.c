// SPDX-License-Identifier: Apache-2.0
#include <absl/base/nullability.h>
#include <openssl/crypto.h>
#include <openssl/rand.h>
#include <stddef.h>

#include "mbedtls/entropy.h"

extern int mbedtls_hardware_poll(void* absl_nullable context,
                                 unsigned char* absl_nullable output,
                                 size_t size, size_t* absl_nullable produced);

// Share the SDK's verified native entropy contract with the modern TLS port.
// RAND_add receives a full-strength estimate only after the complete fill.
int RAND_poll(void) {
  unsigned char seed[32];
  size_t produced = 0;
  int result = mbedtls_hardware_poll(NULL, seed, sizeof(seed), &produced);
  int complete = result == 0 && produced == sizeof(seed);
  if (complete) {
    RAND_add(seed, sizeof(seed), sizeof(seed));
  }
  OPENSSL_cleanse(seed, sizeof(seed));
  return complete;
}
