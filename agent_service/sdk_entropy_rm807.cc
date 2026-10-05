// SPDX-License-Identifier: Apache-2.0
// Research-only RM-807/Belle adapter. Deliberately excluded from SDK archives.
// The named emulator ROM's Math::RandomL(TDes8&) wrapper reaches SWI 0x10A.
// This private executive number must not be assumed for another firmware.
#include <e32std.h>

#include "mbedtls/entropy.h"
#include "mbedtls/platform_util.h"

extern "C" TInt SymbianRm807SecureRandom(TDes8* output);

extern "C" int mbedtls_hardware_poll(void* data, unsigned char* output,
                                     size_t len, size_t* olen) {
  (void)data;
  if (olen == nullptr) {
    return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
  }
  *olen = 0;
  if (output == nullptr || len == 0 || len > 1024) {
    return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
  }
  TPtr8 random(output, static_cast<TInt>(len), static_cast<TInt>(len));
  const TInt result = SymbianRm807SecureRandom(&random);
  if (result != KErrNone) {
    mbedtls_platform_zeroize(output, len);
    return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
  }
  *olen = len;
  return 0;
}
