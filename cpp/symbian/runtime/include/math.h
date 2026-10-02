#ifndef SYMBIAN_RUNTIME_MATH_H_
#define SYMBIAN_RUNTIME_MATH_H_

// OpenC's libm_aliases.h rewrites distinct C++ math overload names (for
// example nexttoward to nextafter). Its declarations remain available, but
// the macro aliases must not alter modern libc++ definitions.
#ifdef __cplusplus
#define __LIBM_ALIASES_H__
#endif
#include_next <math.h>

#ifdef __SYMBIAN32__
#ifdef __cplusplus
extern "C" {
#endif
// OpenC exposes scalbnf on-device but suppresses its ldexpf alias in the
// Symbian header. The SDK supplies the standard entry point in math_c.cc.
float ldexpf(float value, int exponent);
long double ldexpl(long double value, int exponent);
double nan(const char* tag);
float nanf(const char* tag);
#ifdef __cplusplus
}
#endif
#endif

#endif  // SYMBIAN_RUNTIME_MATH_H_
