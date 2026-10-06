// SPDX-License-Identifier: Apache-2.0
#include <absl/base/nullability.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/socket.h>

#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "symbian_mbedtls/socket_bio.h"

int symbian_mbedtls_socket_bio_attach(
    symbian_mbedtls_socket_bio* absl_nullable bio, int fd) {
  if (bio == NULL || fd < 0)
    return -1;
  const int flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
    return -1;
  bio->fd = fd;
  __atomic_store_n(&bio->cancelled, 0, __ATOMIC_RELEASE);
  return 0;
}

void symbian_mbedtls_socket_bio_cancel(
    symbian_mbedtls_socket_bio* absl_nullable bio) {
  if (bio != NULL)
    __atomic_store_n(&bio->cancelled, 1, __ATOMIC_RELEASE);
}

static int bio_ready(const symbian_mbedtls_socket_bio* absl_nullable bio) {
  if (bio == NULL || bio->fd < 0)
    return MBEDTLS_ERR_NET_INVALID_CONTEXT;
  if (__atomic_load_n(&bio->cancelled, __ATOMIC_ACQUIRE))
    return MBEDTLS_ERR_NET_CONN_RESET;
  return 0;
}

int symbian_mbedtls_socket_bio_send(void* absl_nullable context,
                                    const unsigned char* absl_nullable data,
                                    size_t size) {
  symbian_mbedtls_socket_bio* absl_nullable bio =
      (symbian_mbedtls_socket_bio*)context;
  const int ready = bio_ready(bio);
  if (ready != 0)
    return ready;
  if (data == NULL || size > INT_MAX)
    return MBEDTLS_ERR_NET_INVALID_CONTEXT;
  const int result = (int)send(bio->fd, data, size, 0);
  if (result >= 0)
    return result;
  if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
    return MBEDTLS_ERR_SSL_WANT_WRITE;
  return MBEDTLS_ERR_NET_SEND_FAILED;
}

int symbian_mbedtls_socket_bio_recv(void* absl_nullable context,
                                    unsigned char* absl_nullable data,
                                    size_t size) {
  symbian_mbedtls_socket_bio* absl_nullable bio =
      (symbian_mbedtls_socket_bio*)context;
  const int ready = bio_ready(bio);
  if (ready != 0)
    return ready;
  if (data == NULL || size > INT_MAX)
    return MBEDTLS_ERR_NET_INVALID_CONTEXT;
  const int result = (int)recv(bio->fd, data, size, 0);
  if (result >= 0)
    return result;
  if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
    return MBEDTLS_ERR_SSL_WANT_READ;
  return MBEDTLS_ERR_NET_RECV_FAILED;
}
