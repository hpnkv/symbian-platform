// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

#include <wchar.h>

#include "abi.h"

namespace {

static_assert(sizeof(mbstate_t) == sizeof(std::uint64_t));
static_assert(sizeof(wchar_t) == 2);

constexpr std::size_t kFailure = static_cast<std::size_t>(-1);
constexpr std::size_t kIncomplete = static_cast<std::size_t>(-2);

mbstate_t* absl_nonnull ImplicitState(int slot) {
  return static_cast<mbstate_t*>(SymbianRuntimeMbState(slot));
}

void Reset(mbstate_t* absl_nonnull state) {
  std::memset(state, 0, sizeof(*state));
}

int SequenceLength(std::uint8_t first) {
  if (first < 0x80) {
    return 1;
  }
  if (first >= 0xC2 && first <= 0xDF) {
    return 2;
  }
  if (first >= 0xE0 && first <= 0xEF) {
    return 3;
  }
  if (first >= 0xF0 && first <= 0xF4) {
    return 4;
  }
  return 0;
}

std::size_t Invalid(mbstate_t* absl_nonnull state) {
  Reset(state);
  errno = EILSEQ;
  return kFailure;
}

std::size_t Encode(char* absl_nonnull output, std::uint32_t scalar) {
  if (scalar < 0x80) {
    output[0] = static_cast<char>(scalar);
    return 1;
  }
  if (scalar < 0x800) {
    output[0] = static_cast<char>(0xC0 | (scalar >> 6));
    output[1] = static_cast<char>(0x80 | (scalar & 0x3F));
    return 2;
  }
  if (scalar < 0x10000) {
    output[0] = static_cast<char>(0xE0 | (scalar >> 12));
    output[1] = static_cast<char>(0x80 | ((scalar >> 6) & 0x3F));
    output[2] = static_cast<char>(0x80 | (scalar & 0x3F));
    return 3;
  }
  output[0] = static_cast<char>(0xF0 | (scalar >> 18));
  output[1] = static_cast<char>(0x80 | ((scalar >> 12) & 0x3F));
  output[2] = static_cast<char>(0x80 | ((scalar >> 6) & 0x3F));
  output[3] = static_cast<char>(0x80 | (scalar & 0x3F));
  return 4;
}

}  // namespace

extern "C" std::size_t mbrtowc(wchar_t* absl_nullable output,
                               const char* absl_nullable input,
                               std::size_t length,
                               mbstate_t* absl_nullable supplied_state) {
  mbstate_t* absl_nonnull state =
      supplied_state == nullptr ? ImplicitState(0) : supplied_state;
  const char* absl_nonnull bytes = input == nullptr ? "" : input;
  if (input == nullptr) {
    length = 1;
  }
  if (length == 0) {
    return kIncomplete;
  }
  int count = state->__count;
  if (count < 0 || count > 3) {
    return Invalid(state);
  }
  std::size_t consumed = 0;
  if (count == 0) {
    const std::uint8_t first = static_cast<std::uint8_t>(bytes[consumed++]);
    const int expected = SequenceLength(first);
    if (expected == 0) {
      return Invalid(state);
    }
    if (expected == 1) {
      if (output != nullptr) {
        *output = static_cast<wchar_t>(first);
      }
      Reset(state);
      return first == 0 ? 0 : 1;
    }
    state->__value.__wchb[count++] = static_cast<char>(first);
  }
  const std::uint8_t first =
      static_cast<std::uint8_t>(state->__value.__wchb[0]);
  const int expected = SequenceLength(first);
  if (expected < 2 || count >= expected) {
    return Invalid(state);
  }
  while (count < expected && consumed < length) {
    const std::uint8_t next = static_cast<std::uint8_t>(bytes[consumed++]);
    if ((next & 0xC0) != 0x80) {
      return Invalid(state);
    }
    state->__value.__wchb[count++] = static_cast<char>(next);
  }
  if (count < expected) {
    state->__count = count;
    return kIncomplete;
  }
  std::uint32_t scalar = first & ((1u << (7 - expected)) - 1);
  for (int index = 1; index < expected; ++index) {
    scalar = (scalar << 6) |
             (static_cast<std::uint8_t>(state->__value.__wchb[index]) & 0x3F);
  }
  // A single 16-bit wchar_t cannot represent a supplementary scalar.
  constexpr std::uint32_t minimum[] = {0, 0, 0x80, 0x800, 0x10000};
  if (scalar < minimum[expected] || scalar > 0xFFFF ||
      (scalar >= 0xD800 && scalar <= 0xDFFF)) {
    return Invalid(state);
  }
  if (output != nullptr) {
    *output = static_cast<wchar_t>(scalar);
  }
  Reset(state);
  return consumed;
}

extern "C" std::size_t mbrlen(const char* absl_nullable input,
                              std::size_t length,
                              mbstate_t* absl_nullable state) {
  return mbrtowc(nullptr, input, length,
                 state == nullptr ? ImplicitState(1) : state);
}

extern "C" int mbtowc(wchar_t* absl_nullable output,
                      const char* absl_nullable input, std::size_t length) {
  if (input == nullptr) {
    return 0;
  }
  mbstate_t state = {};
  const std::size_t result = mbrtowc(output, input, length, &state);
  if (result == kIncomplete) {
    errno = EILSEQ;
    return -1;
  }
  return result == kFailure ? -1 : static_cast<int>(result);
}

extern "C" std::size_t mbsnrtowcs(
    wchar_t* absl_nullable output,
    const char* absl_nullable* absl_nonnull source, std::size_t input_limit,
    std::size_t output_limit, mbstate_t* absl_nullable supplied_state) {
  mbstate_t* absl_nonnull state =
      supplied_state == nullptr ? ImplicitState(2) : supplied_state;
  mbstate_t copy = *state;
  mbstate_t* absl_nonnull working = output == nullptr ? &copy : state;
  const char* absl_nonnull cursor = *source;
  std::size_t count = 0;
  while (input_limit != 0 && (output == nullptr || count < output_limit)) {
    const std::size_t available = input_limit < 4 ? input_limit : 4;
    wchar_t converted = 0;
    const std::size_t result = mbrtowc(&converted, cursor, available, working);
    if (result == kFailure) {
      if (output != nullptr) {
        *source = cursor;
      }
      return kFailure;
    }
    if (result == kIncomplete) {
      if (output != nullptr) {
        *source = cursor + available;
      }
      return count;
    }
    if (result == 0) {
      if (output != nullptr) {
        output[count] = 0;
        *source = nullptr;
      }
      return count;
    }
    if (output != nullptr) {
      output[count] = converted;
    }
    ++count;
    cursor += result;
    input_limit -= result;
  }
  if (output != nullptr) {
    *source = cursor;
  }
  return count;
}

extern "C" std::size_t mbsrtowcs(wchar_t* absl_nullable output,
                                 const char* absl_nullable* absl_nonnull source,
                                 std::size_t output_limit,
                                 mbstate_t* absl_nullable state) {
  return mbsnrtowcs(output, source, std::numeric_limits<std::size_t>::max(),
                    output_limit, state == nullptr ? ImplicitState(3) : state);
}

extern "C" std::size_t wcrtomb(char* absl_nullable output, wchar_t character,
                               mbstate_t* absl_nullable supplied_state) {
  mbstate_t* absl_nonnull state =
      supplied_state == nullptr ? ImplicitState(4) : supplied_state;
  if (output == nullptr) {
    Reset(state);
    return 1;
  }
  const std::uint32_t value = static_cast<std::uint32_t>(character);
  if (state->__count == -1) {
    const std::uint32_t high = static_cast<std::uint32_t>(state->__value.__wch);
    if (value < 0xDC00 || value > 0xDFFF) {
      return Invalid(state);
    }
    const std::uint32_t scalar =
        0x10000 + ((high - 0xD800) << 10) + (value - 0xDC00);
    Reset(state);
    return Encode(output, scalar);
  }
  if (state->__count != 0) {
    return Invalid(state);
  }
  if (value >= 0xD800 && value <= 0xDBFF) {
    state->__count = -1;
    state->__value.__wch = static_cast<int>(value);
    return 0;
  }
  if (value >= 0xDC00 && value <= 0xDFFF) {
    return Invalid(state);
  }
  return Encode(output, value);
}

extern "C" std::size_t wcsnrtombs(
    char* absl_nullable output,
    const wchar_t* absl_nullable* absl_nonnull source, std::size_t input_limit,
    std::size_t output_limit, mbstate_t* absl_nullable supplied_state) {
  mbstate_t* absl_nonnull state =
      supplied_state == nullptr ? ImplicitState(5) : supplied_state;
  mbstate_t copy = *state;
  mbstate_t* absl_nonnull working = output == nullptr ? &copy : state;
  const wchar_t* absl_nonnull cursor = *source;
  std::size_t bytes = 0;
  for (std::size_t index = 0; index < input_limit; ++index) {
    char encoded[4] = {};
    mbstate_t next_state = *working;
    const std::size_t count = wcrtomb(encoded, cursor[index], &next_state);
    if (count == kFailure) {
      if (output != nullptr) {
        *source = cursor + index;
      }
      return kFailure;
    }
    if (output != nullptr && count > output_limit - bytes) {
      *source = cursor + index;
      return bytes;
    }
    *working = next_state;
    if (cursor[index] == 0) {
      if (output != nullptr) {
        output[bytes] = 0;
        *source = nullptr;
      }
      return bytes;
    }
    if (output != nullptr) {
      std::memcpy(output + bytes, encoded, count);
    }
    bytes += count;
  }
  if (output != nullptr) {
    *source = cursor + input_limit;
  }
  return bytes;
}

extern "C" std::size_t wcsrtombs(
    char* absl_nullable output,
    const wchar_t* absl_nullable* absl_nonnull source, std::size_t output_limit,
    mbstate_t* absl_nullable state) {
  return wcsnrtombs(output, source, std::numeric_limits<std::size_t>::max(),
                    output_limit, state == nullptr ? ImplicitState(6) : state);
}

extern "C" int mbsinit(const mbstate_t* absl_nullable state) {
  return state == nullptr || state->__count == 0;
}
