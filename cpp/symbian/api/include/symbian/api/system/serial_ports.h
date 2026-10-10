// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_API_SYSTEM_SERIAL_PORTS_H_
#define SYMBIAN_API_SYSTEM_SERIAL_PORTS_H_

#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/statusor.h"

namespace symbian::api::system {

/** One OS serial module and its contiguous range of port units. */
struct SerialPortRange {
  std::u16string module;
  std::u16string name;
  std::u16string description;
  std::uint32_t first_unit = 0;
  std::uint32_t last_unit = 0;
};

/** Enumerate OS serial ports without opening or changing any port. */
absl::StatusOr<std::vector<SerialPortRange>> ListSerialPortRanges();

}  // namespace symbian::api::system

#endif  // SYMBIAN_API_SYSTEM_SERIAL_PORTS_H_
