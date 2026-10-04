// SPDX-License-Identifier: Apache-2.0
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <gtest/gtest.h>

#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "symbian_mbedtls/socket_bio.h"

TEST(SocketBio, NonblockingReadWriteAndCancel) {
  int pair[2];
  ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, pair), 0);
  symbian_mbedtls_socket_bio bio{};
  ASSERT_EQ(symbian_mbedtls_socket_bio_attach(&bio, pair[0]), 0);
  EXPECT_NE(fcntl(pair[0], F_GETFL, 0) & O_NONBLOCK, 0);

  unsigned char output[4] = {};
  EXPECT_EQ(symbian_mbedtls_socket_bio_recv(&bio, output, sizeof(output)),
            MBEDTLS_ERR_SSL_WANT_READ);
  const unsigned char input[] = {'t', 'e', 's', 't'};
  EXPECT_EQ(symbian_mbedtls_socket_bio_send(&bio, input, sizeof(input)), 4);
  EXPECT_EQ(recv(pair[1], output, sizeof(output), 0), 4);
  EXPECT_EQ(output[0], 't');

  symbian_mbedtls_socket_bio_cancel(&bio);
  EXPECT_EQ(symbian_mbedtls_socket_bio_recv(&bio, output, sizeof(output)),
            MBEDTLS_ERR_NET_CONN_RESET);
  EXPECT_EQ(symbian_mbedtls_socket_bio_send(&bio, input, sizeof(input)),
            MBEDTLS_ERR_NET_CONN_RESET);
  close(pair[1]);
  close(pair[0]);
}

TEST(SocketBio, RejectsInvalidDescriptor) {
  symbian_mbedtls_socket_bio bio{};
  EXPECT_EQ(symbian_mbedtls_socket_bio_attach(&bio, -1), -1);
  EXPECT_EQ(symbian_mbedtls_socket_bio_recv(&bio, nullptr, 1),
            MBEDTLS_ERR_NET_INVALID_CONTEXT);
}
