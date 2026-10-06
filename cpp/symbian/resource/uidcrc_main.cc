// Copyright 2026 The Symbian SDK Authors
// SPDX-License-Identifier: Apache-2.0

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

#include <absl/base/nullability.h>

#include "symbian/analysis/checksum.h"

int main(int argc, char* absl_nullable* absl_nonnull argv) {
  if (argc != 5) {
    std::fprintf(stderr, "usage: uidcrc UID1 UID2 UID3 OUTPUT\n");
    return 2;
  }
  std::string header(16, '\0');
  for (int index = 0; index < 3; ++index) {
    errno = 0;
    char* absl_nullable end = nullptr;
    const unsigned long value = std::strtoul(argv[index + 1], &end, 0);
    if (errno != 0 || end == argv[index + 1] || *end != '\0' ||
        value > UINT32_MAX) {
      std::fprintf(stderr, "invalid UID argument\n");
      return 2;
    }
    for (int byte = 0; byte < 4; ++byte) {
      header[index * 4 + byte] = static_cast<char>(value >> (8 * byte));
    }
  }
  const uint32_t checksum = symbian::analysis::internal::UidChecksum(header);
  for (int byte = 0; byte < 4; ++byte) {
    header[12 + byte] = static_cast<char>(checksum >> (8 * byte));
  }
  std::FILE* absl_nullable output = std::fopen(argv[4], "wb");
  if (output == nullptr) {
    std::fprintf(stderr, "cannot open output\n");
    return 1;
  }
  const bool written =
      std::fwrite(header.data(), 1, header.size(), output) == header.size();
  const bool closed = std::fclose(output) == 0;
  return written && closed ? 0 : 1;
}
