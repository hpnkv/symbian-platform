#include "common.h"

#ifdef __SYMBIAN32__

#include <unistd.h>

#ifndef __ARMCC_4_0__
extern int __aeabi_uidivmod(unsigned int a, unsigned int b);
extern int __aeabi_idivmod(int a, int b);
int __aeabi_idiv(int a, int b)
{
	return __aeabi_idivmod(a, b);
}

int __aeabi_uidiv(unsigned int a, unsigned int b)
{
	return __aeabi_uidivmod(a, b);
}
#endif

#endif

#ifdef MBEDTLS_ENTROPY_HARDWARE_ALT

#include "mbedtls/entropy.h"

EXPORT_C int mbedtls_hardware_poll(void *data, unsigned char *output, size_t len, size_t *olen)
{
    (void) data;
    (void) output;
    (void) len;
    *olen = 0;
    /* There is no verified guest entropy source yet. Fail closed. */
    return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
}

#endif
