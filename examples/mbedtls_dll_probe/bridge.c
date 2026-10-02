#include <mbedtls/sha256.h>

__attribute__((visibility("default")))
int MbedSha256(const unsigned char *input, unsigned size,
               unsigned char *output) {
  return mbedtls_sha256(input, size, output, 0);
}
