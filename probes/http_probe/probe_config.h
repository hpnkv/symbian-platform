// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.
#pragma once

#include <array>
#include <cstdint>

#include <absl/base/nullability.h>

struct ProbeCase {
  const char* absl_nonnull hostname;
  std::array<std::uint8_t, 4> address;
  std::uint16_t port;
  int tls_version;  // 0, 12, 13.
  bool http2;
  bool reject_certificate;
};

inline constexpr ProbeCase kCases[] = {
    {"localhost", {127, 0, 0, 1}, 39105, 0, false, false}};
inline constexpr char kRoots[] = "";
inline constexpr char kServerCertificate[] = "";
inline constexpr char kServerKey[] = "";
