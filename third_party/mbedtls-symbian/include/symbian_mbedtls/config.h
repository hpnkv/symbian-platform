// SPDX-License-Identifier: Apache-2.0
#ifndef SYMBIAN_MBEDTLS_CONFIG_H_
#define SYMBIAN_MBEDTLS_CONFIG_H_

#define MBEDTLS_SSL_SRV_C

// Single event-thread ownership until the SDK concurrency backend is verified.
#undef MBEDTLS_THREADING_C
#undef MBEDTLS_THREADING_PTHREAD
#undef MBEDTLS_THREADING_IMPL
#undef MBEDTLS_NET_C
#undef MBEDTLS_TIMING_C
#undef MBEDTLS_FS_IO
#undef MBEDTLS_SELF_TEST
// Do not inherit the port's ordinary memset-based secret erasure.
#define MBEDTLS_PLATFORM_ZEROIZE_ALT

#ifdef SYMBIAN_MBEDTLS_GUEST
// These are required services, not successful no-op implementations. The final
// app link fails until it provides a trusted entropy source and UTC conversion.
#include "symbian_mbedtls/platform.h"
#define MBEDTLS_PLATFORM_MEMORY
#define MBEDTLS_PLATFORM_CALLOC_MACRO symbian_mbedtls_calloc
#define MBEDTLS_PLATFORM_FREE_MACRO symbian_mbedtls_free
#define MBEDTLS_PLATFORM_GMTIME_R_ALT
#define MBEDTLS_PLATFORM_TIME_MACRO symbian_mbedtls_time
#define MBEDTLS_NO_PLATFORM_ENTROPY
#define MBEDTLS_ENTROPY_HARDWARE_ALT
#else
#define MBEDTLS_DEBUG_C
// Native verification uses the host entropy source, not library/symbian.c.
#undef MBEDTLS_ENTROPY_HARDWARE_ALT
#endif

#endif  // SYMBIAN_MBEDTLS_CONFIG_H_
