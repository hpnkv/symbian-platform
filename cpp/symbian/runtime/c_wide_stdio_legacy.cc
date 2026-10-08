// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include <cerrno>
#include <cstdio>
#include <cwchar>

#include <absl/base/nullability.h>

extern "C" wint_t getwc(FILE* absl_nonnull stream) {
  mbstate_t state = {};
  wchar_t value = 0;
  for (int index = 0; index < 4; ++index) {
    const int next = getc(stream);
    if (next == EOF) {
      if (index != 0) {
        errno = EILSEQ;
      }
      return WEOF;
    }
    const char byte = static_cast<char>(next);
    const std::size_t converted = mbrtowc(&value, &byte, 1, &state);
    if (converted == static_cast<std::size_t>(-1)) {
      return WEOF;
    }
    if (converted != static_cast<std::size_t>(-2)) {
      return static_cast<wint_t>(value);
    }
  }
  errno = EILSEQ;
  return WEOF;
}

extern "C" wint_t fputwc(wchar_t value, FILE* absl_nonnull stream) {
  mbstate_t state = {};
  char encoded[4] = {};
  const std::size_t count = wcrtomb(encoded, value, &state);
  if (count == static_cast<std::size_t>(-1)) {
    return WEOF;
  }
  if (count == 0) {
    errno = EILSEQ;
    return WEOF;
  }
  return fwrite(encoded, 1, count, stream) == count
             ? static_cast<wint_t>(value)
             : WEOF;
}

extern "C" wint_t ungetwc(wint_t value, FILE* absl_nonnull stream) {
  // The old ESTLIB stream only guarantees one byte of pushback.
  if (value == WEOF || value > 0x7F) {
    errno = EILSEQ;
    return WEOF;
  }
  return ungetc(static_cast<int>(value), stream) == EOF ? WEOF : value;
}
