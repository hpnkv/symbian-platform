// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>

#include <absl/base/nullability.h>

namespace {

bool IsSpace(char value) {
  return value == ' ' || value == '\t' || value == '\n' || value == '\r' ||
         value == '\f' || value == '\v';
}

char Lower(char value) {
  return value >= 'A' && value <= 'Z' ? static_cast<char>(value + 32) : value;
}

bool StartsWithIgnoringCase(const char* absl_nonnull text,
                            const char* absl_nonnull word) {
  while (*word != '\0') {
    if (*text == '\0') {
      return false;
    }
    if (Lower(*text++) != Lower(*word++)) {
      return false;
    }
  }
  return true;
}

int DecimalDigit(char value) {
  return value >= '0' && value <= '9' ? value - '0' : -1;
}

int HexDigit(char value) {
  if (value >= '0' && value <= '9') {
    return value - '0';
  }
  value = Lower(value);
  return value >= 'a' && value <= 'f' ? value - 'a' + 10 : -1;
}

int ParseExponent(const char* absl_nonnull* absl_nonnull cursor, char marker) {
  const char* absl_nonnull start = *cursor;
  if (Lower(**cursor) != marker) {
    return 0;
  }
  ++*cursor;
  bool negative = false;
  if (**cursor == '+' || **cursor == '-') {
    negative = **cursor == '-';
    ++*cursor;
  }
  if (DecimalDigit(**cursor) < 0) {
    *cursor = start;
    return 0;
  }
  int exponent = 0;
  while (DecimalDigit(**cursor) >= 0) {
    const int digit = DecimalDigit(**cursor);
    exponent = exponent > 10000 ? 10000 : exponent * 10 + digit;
    ++*cursor;
  }
  return negative ? -exponent : exponent;
}

double ScaleDecimal(double value, int exponent) {
  if (exponent > 400) {
    return __builtin_inf();
  }
  if (exponent < -400) {
    return 0.0;
  }
  unsigned int remaining =
      static_cast<unsigned int>(exponent < 0 ? -exponent : exponent);
  double power = exponent < 0 ? 0.1 : 10.0;
  while (remaining != 0) {
    if ((remaining & 1U) != 0) {
      value *= power;
    }
    remaining >>= 1;
    if (remaining != 0) {
      power *= power;
    }
  }
  return value;
}

double ParseNumber(const char* absl_nonnull input,
                   char* absl_nullable* absl_nullable end) {
  const char* absl_nonnull cursor = input;
  while (IsSpace(*cursor)) {
    ++cursor;
  }
  const bool negative = *cursor == '-';
  if (*cursor == '-' || *cursor == '+') {
    ++cursor;
  }
  if (StartsWithIgnoringCase(cursor, "inf")) {
    cursor += 3;
    if (StartsWithIgnoringCase(cursor, "inity")) {
      cursor += 5;
    }
    if (end != nullptr) {
      *end = const_cast<char*>(cursor);
    }
    return negative ? -__builtin_inf() : __builtin_inf();
  }
  if (StartsWithIgnoringCase(cursor, "nan")) {
    cursor += 3;
    if (*cursor == '(') {
      const char* absl_nonnull payload = cursor + 1;
      while ((*payload >= 'a' && *payload <= 'z') ||
             (*payload >= 'A' && *payload <= 'Z') ||
             (*payload >= '0' && *payload <= '9') || *payload == '_') {
        ++payload;
      }
      if (*payload == ')') {
        cursor = payload + 1;
      }
    }
    if (end != nullptr) {
      *end = const_cast<char*>(cursor);
    }
    const double value = __builtin_nan("");
    return negative ? -value : value;
  }

  const bool hexadecimal = cursor[0] == '0' && Lower(cursor[1]) == 'x' &&
                           (HexDigit(cursor[2]) >= 0 ||
                            (cursor[2] == '.' && HexDigit(cursor[3]) >= 0));
  if (hexadecimal) {
    cursor += 2;
  }
  const int radix = hexadecimal ? 16 : 10;
  std::uint64_t mantissa = 0;
  int scale = 0;
  int significant_digits = 0;
  bool saw_digit = false;
  bool fraction = false;
  while (true) {
    if (*cursor == '.' && !fraction) {
      fraction = true;
      ++cursor;
      continue;
    }
    const int digit = hexadecimal ? HexDigit(*cursor) : DecimalDigit(*cursor);
    if (digit < 0) {
      break;
    }
    saw_digit = true;
    if (mantissa == 0 && digit == 0) {
      if (fraction) {
        scale -= hexadecimal ? 4 : 1;
      }
    } else if (significant_digits < (hexadecimal ? 15 : 18)) {
      mantissa = mantissa * static_cast<unsigned int>(radix) +
                 static_cast<unsigned int>(digit);
      ++significant_digits;
      if (fraction) {
        scale -= hexadecimal ? 4 : 1;
      }
    } else if (!fraction) {
      scale += hexadecimal ? 4 : 1;
    }
    ++cursor;
  }
  if (!saw_digit) {
    if (end != nullptr) {
      *end = const_cast<char*>(input);
    }
    return 0.0;
  }
  scale += ParseExponent(&cursor, hexadecimal ? 'p' : 'e');
  if (end != nullptr) {
    *end = const_cast<char*>(cursor);
  }
  double value = static_cast<double>(mantissa);
  if (mantissa != 0) {
    value = hexadecimal ? std::ldexp(value, scale)
                        : ScaleDecimal(value, scale);
    if (__builtin_isinf(value) || value == 0.0) {
      errno = ERANGE;
    }
  }
  return negative ? -value : value;
}

}  // namespace

extern "C" double strtod(const char* absl_nonnull input,
                          char* absl_nullable* absl_nullable end) {
  return ParseNumber(input, end);
}

extern "C" float strtof(const char* absl_nonnull input,
                         char* absl_nullable* absl_nullable end) {
  const double parsed = ParseNumber(input, end);
  const float value = static_cast<float>(parsed);
  if (__builtin_isinf(value) && !__builtin_isinf(parsed)) {
    errno = ERANGE;
  }
  return value;
}
