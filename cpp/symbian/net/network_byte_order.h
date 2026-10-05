// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.
// nghttp2 needs byte order conversions, not a POSIX socket implementation.
#ifndef SYMBIAN_WEBSOCKET_NETWORK_BYTE_ORDER_H_
#define SYMBIAN_WEBSOCKET_NETWORK_BYTE_ORDER_H_
#ifdef __SYMBIAN32__
#include <e32def.h>
#include <sys/types.h>
#endif
#include <stdint.h>
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define htonl(value) __builtin_bswap32((uint32_t)(value))
#define htons(value) __builtin_bswap16((uint16_t)(value))
#else
#define htonl(value) ((uint32_t)(value))
#define htons(value) ((uint16_t)(value))
#endif
#define ntohl(value) htonl(value)
#define ntohs(value) htons(value)
#endif
