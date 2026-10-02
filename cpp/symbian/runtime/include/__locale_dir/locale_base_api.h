#ifndef SYMBIAN_RUNTIME_LOCALE_BASE_API_H_
#define SYMBIAN_RUNTIME_LOCALE_BASE_API_H_

// LLVM libc++ 23's locale substrate adapted to OpenC's C-locale interface.
// The initial profile supports only C/POSIX and uses the process C locale.
#include <__config>
#include <cerrno>
#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <cstring>

_LIBCPP_BEGIN_NAMESPACE_STD

namespace __locale {

struct ClassicLocale {};

using __locale_t = ClassicLocale*;

#define _LIBCPP_COLLATE_MASK 1
#define _LIBCPP_CTYPE_MASK 2
#define _LIBCPP_MONETARY_MASK 4
#define _LIBCPP_NUMERIC_MASK 8
#define _LIBCPP_TIME_MASK 16
#define _LIBCPP_MESSAGES_MASK 32
#define _LIBCPP_ALL_MASK 63
#define _LIBCPP_LC_ALL LC_ALL

#if defined(_LIBCPP_BUILDING_LIBRARY)
using __lconv_t = std::lconv;
inline ClassicLocale classic_locale;

inline __locale_t __newlocale(int mask, const char* name, __locale_t base) {
  if (mask <= 0 || (mask & ~_LIBCPP_ALL_MASK) != 0 ||
      (base != nullptr && base != &classic_locale) || name == nullptr ||
      (std::strcmp(name, "C") != 0 && std::strcmp(name, "POSIX") != 0)) {
    errno = EINVAL;
    return nullptr;
  }
  return &classic_locale;
}

inline void __freelocale(__locale_t) {}

inline char* __setlocale(int category, const char* name) {
  if (category < LC_ALL || category > LC_MESSAGES ||
      (name != nullptr && std::strcmp(name, "C") != 0 &&
       std::strcmp(name, "POSIX") != 0)) {
    errno = EINVAL;
    return nullptr;
  }
  return const_cast<char*>("C");
}

inline __lconv_t* __localeconv(__locale_t&) {
  return std::localeconv();
}

inline const char* __get_locale_encoding(__locale_t) {
  return "US-ASCII";
}
#endif

}  // namespace __locale

_LIBCPP_END_NAMESPACE_STD

// Preserve the original LLVM implementations of the C-locale operations.
#include <__locale_dir/support/no_locale/characters.h>
#include <__locale_dir/support/no_locale/conversions.h>
#include <__locale_dir/support/no_locale/formatting.h>
#include <__locale_dir/support/no_locale/strtonum.h>

#endif  // SYMBIAN_RUNTIME_LOCALE_BASE_API_H_
