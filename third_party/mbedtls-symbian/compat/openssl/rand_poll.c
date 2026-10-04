// SPDX-License-Identifier: Apache-2.0
#include <openssl/crypto.h>
#include <openssl/rand.h>
#include <stddef.h>

// Required application/SDK service. Success means all requested bytes came
// from a verified strong source. The linker rejects a missing implementation.
extern int symbian_tls_entropy_poll(unsigned char *output, size_t size);

int RAND_poll(void) {
  unsigned char seed[32];
  int result = symbian_tls_entropy_poll(seed, sizeof(seed));
  if (result == 0) {
    RAND_add(seed, sizeof(seed), sizeof(seed));
  }
  OPENSSL_cleanse(seed, sizeof(seed));
  return result == 0;
}
