#include <cerrno>
#include <cstring>

#include <__locale_dir/locale_base_api.h>
#include <absl/base/nullability.h>

int SymbianRuntimeLocaleApiProbe() {
  namespace locale = std::__symbian::__locale;
  errno = 0;
  if (locale::__newlocale(_LIBCPP_ALL_MASK, "not-a-locale", nullptr) !=
          nullptr ||
      errno != EINVAL) {
    return -196;
  }
  errno = 0;
  if (locale::__newlocale(0, "C", nullptr) != nullptr || errno != EINVAL) {
    return -197;
  }
  errno = 0;
  if (locale::__setlocale(LC_NUMERIC, "not-a-locale") != nullptr ||
      errno != EINVAL) {
    return -198;
  }
  const char* absl_nullable classic = locale::__setlocale(LC_NUMERIC, "C");
  if (classic == nullptr || std::strcmp(classic, "C") != 0) {
    return -199;
  }
  return 0;
}
