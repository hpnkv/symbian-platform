// SPDX-License-Identifier: Apache-2.0
// Disposable RM-807 emulator diagnostic; not part of the SDK TLS surface.
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>
#include <symbian_mbedtls/socket_bio.h>

__attribute__((visibility("default"))) int MbedConnectedSocketProbe(int port) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) return -151;
  struct sockaddr_in address = {0};
  address.sin_family = AF_INET;
  address.sin_port = (unsigned short)(((unsigned)port << 8) |
                                       ((unsigned)port >> 8));
  address.sin_addr.s_addr = 0x0100007fu;
  if (connect(fd, (struct sockaddr*)&address, sizeof(address)) != 0) {
    close(fd);
    return -152;
  }
  symbian_mbedtls_socket_bio bio = {0, 0};
  if (symbian_mbedtls_socket_bio_attach(&bio, fd) != 0) {
    close(fd);
    return -153;
  }
  unsigned char data = 0;
  int result = symbian_mbedtls_socket_bio_recv(&bio, &data, 1);
  if (result != MBEDTLS_ERR_SSL_WANT_READ) {
    close(fd);
    return -154;
  }
  for (int i = 0; i < 100000; ++i) {
    result = symbian_mbedtls_socket_bio_recv(&bio, &data, 1);
    if (result != MBEDTLS_ERR_SSL_WANT_READ) break;
  }
  if (result != 1 || data != 'R') {
    close(fd);
    return -156;
  }
  symbian_mbedtls_socket_bio_cancel(&bio);
  result = symbian_mbedtls_socket_bio_recv(&bio, &data, 1);
  close(fd);
  return result == MBEDTLS_ERR_NET_CONN_RESET ? 0 : -157;
}
