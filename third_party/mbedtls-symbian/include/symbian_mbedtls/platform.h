// SPDX-License-Identifier: Apache-2.0
#ifndef SYMBIAN_MBEDTLS_PLATFORM_H_
#define SYMBIAN_MBEDTLS_PLATFORM_H_
#if defined(__SYMBIAN32__)
#include <e32def.h>
#endif
#include <stddef.h>
#include <time.h>
#ifdef __cplusplus
extern "C" {
#endif
// Allocation is bounded by the Symbian signed 32-bit heap size. Overflow and
// exhaustion return NULL; the library propagates its existing allocation errors.
void* symbian_mbedtls_calloc(size_t count, size_t size);
void symbian_mbedtls_free(void* pointer);
// Implement with real UTC time. Certificate date verification remains enabled.
time_t symbian_mbedtls_time(time_t* output);
// Returns NULL for an unavailable or implausible UTC clock.
struct tm* symbian_mbedtls_utc_gmtime_r(const time_t* input,
                                       struct tm* output);
#ifdef __cplusplus
}
#endif
#endif  // SYMBIAN_MBEDTLS_PLATFORM_H_
