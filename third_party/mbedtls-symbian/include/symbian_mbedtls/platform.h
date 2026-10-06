// SPDX-License-Identifier: Apache-2.0
#ifndef SYMBIAN_MBEDTLS_PLATFORM_H_
#define SYMBIAN_MBEDTLS_PLATFORM_H_
#include <absl/base/nullability.h>
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
void* absl_nullable symbian_mbedtls_calloc(size_t count, size_t size);
void symbian_mbedtls_free(void* absl_nullable pointer);
// Implement with real UTC time. Certificate date verification remains enabled.
time_t symbian_mbedtls_time(time_t* absl_nullable output);
// Returns NULL for an unavailable or implausible UTC clock.
struct tm* absl_nullable symbian_mbedtls_utc_gmtime_r(
    const time_t* absl_nullable input, struct tm* absl_nullable output);
#ifdef __cplusplus
}
#endif
#endif  // SYMBIAN_MBEDTLS_PLATFORM_H_
