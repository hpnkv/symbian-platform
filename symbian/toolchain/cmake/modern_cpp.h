#ifndef SYMBIAN_MODERN_CPP_H_
#define SYMBIAN_MODERN_CPP_H_

#include <new>

#include <e32def.h>

// libc++ owns standard allocation declarations. The original platform headers
// provide leaving overloads, but must not redefine standard placement new.
#undef __NO_THROW
#define __NO_THROW noexcept
#define __SYMBIAN_STDCPP_SUPPORT__
#define __PLACEMENT_NEW_INLINE
#define __PLACEMENT_VEC_NEW_INLINE

#endif  // SYMBIAN_MODERN_CPP_H_
