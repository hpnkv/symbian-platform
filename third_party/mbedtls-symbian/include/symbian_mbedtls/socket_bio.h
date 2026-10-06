// SPDX-License-Identifier: Apache-2.0
#ifndef SYMBIAN_MBEDTLS_SOCKET_BIO_H_
#define SYMBIAN_MBEDTLS_SOCKET_BIO_H_

#include <absl/base/nullability.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// The caller owns fd and closes it after all callbacks have stopped. Attach
// makes the connected socket nonblocking; a failed attach leaves it unchanged.
typedef struct symbian_mbedtls_socket_bio {
  int fd;
  int cancelled;
} symbian_mbedtls_socket_bio;

int symbian_mbedtls_socket_bio_attach(
    symbian_mbedtls_socket_bio* absl_nullable bio, int fd);
void symbian_mbedtls_socket_bio_cancel(
    symbian_mbedtls_socket_bio* absl_nullable bio);
int symbian_mbedtls_socket_bio_send(void* absl_nullable context,
                                    const unsigned char* absl_nullable data,
                                    size_t size);
int symbian_mbedtls_socket_bio_recv(void* absl_nullable context,
                                    unsigned char* absl_nullable data,
                                    size_t size);

#ifdef __cplusplus
}
#endif
#endif  // SYMBIAN_MBEDTLS_SOCKET_BIO_H_
