// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include <cerrno>
#include <cstdarg>
#include <cstddef>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <limits>

#include <absl/base/nullability.h>

#define STB_SPRINTF_IMPLEMENTATION
#include "stb_sprintf.h"

extern "C" void perror(const char* absl_nullable prefix) {
  const int saved_errno = errno;
  std::fprintf(stderr, "%s%s%s\n", prefix == nullptr ? "" : prefix,
               prefix != nullptr && *prefix != '\0' ? ": " : "",
               std::strerror(saved_errno));
  errno = saved_errno;
}

extern "C" int vsnprintf(char* absl_nullable output, std::size_t capacity,
                          const char* absl_nonnull format, va_list arguments) {
  if (format == nullptr || (capacity != 0 && output == nullptr)) {
    errno = EINVAL;
    return -1;
  }
  if (capacity == 0) {
    return stbsp_vsnprintf(nullptr, 0, format, arguments);
  }
  const std::size_t maximum = static_cast<std::size_t>(
      std::numeric_limits<int>::max());
  return stbsp_vsnprintf(output,
                         static_cast<int>(capacity > maximum ? maximum
                                                              : capacity),
                         format, arguments);
}

extern "C" int snprintf(char* absl_nullable output, std::size_t capacity,
                         const char* absl_nonnull format, ...) {
  va_list arguments;
  va_start(arguments, format);
  const int result = vsnprintf(output, capacity, format, arguments);
  va_end(arguments);
  return result;
}

extern "C" int asprintf(char* absl_nullable* absl_nullable output,
                         const char* absl_nonnull format, ...) {
  if (output == nullptr || format == nullptr) {
    errno = EINVAL;
    return -1;
  }
  *output = nullptr;
  va_list arguments;
  va_start(arguments, format);
  va_list measurement;
  va_copy(measurement, arguments);
  const int length = vsnprintf(nullptr, 0, format, measurement);
  va_end(measurement);
  if (length < 0 ||
      static_cast<std::size_t>(length) ==
          std::numeric_limits<std::size_t>::max()) {
    va_end(arguments);
    errno = EOVERFLOW;
    return -1;
  }
  char* absl_nullable buffer =
      static_cast<char*>(malloc(static_cast<std::size_t>(length) + 1));
  if (buffer == nullptr) {
    va_end(arguments);
    errno = ENOMEM;
    return -1;
  }
  const int written =
      vsnprintf(buffer, static_cast<std::size_t>(length) + 1, format, arguments);
  va_end(arguments);
  if (written != length) {
    free(buffer);
    errno = EINVAL;
    return -1;
  }
  *output = buffer;
  return length;
}
