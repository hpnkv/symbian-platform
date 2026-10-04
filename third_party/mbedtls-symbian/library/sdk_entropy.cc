// SPDX-License-Identifier: Apache-2.0
// Research candidate only. Not linked into SDK archives: EABI does not export
// TTrap::Trap/UnTrap, and the SDK has no guest C++ exception unwinder.
#include <e32math.h>

#include "mbedtls/entropy.h"
#include "mbedtls/platform_util.h"

extern "C" int mbedtls_hardware_poll(void* data, unsigned char* output,
                                     size_t len, size_t* olen) {
  (void)data;
  if (olen == nullptr)
    return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
  *olen = 0;
  if (output == nullptr || len == 0 || len > 1024) {
    return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
  }
  TPtr8 random(output, static_cast<TInt>(len), static_cast<TInt>(len));
  TRAPD(error, Math::RandomL(random));
  if (error != KErrNone) {
    mbedtls_platform_zeroize(output, len);
    return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
  }
  *olen = len;
  return 0;
}
