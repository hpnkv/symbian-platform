#ifndef SYMBIAN_NATIVE_CPP_COMPAT_H_
#define SYMBIAN_NATIVE_CPP_COMPAT_H_

#include <new>

#include <e32def.h>

// libc++ declares the standard allocation operators. Symbian's public
// e32cmn.h must keep its leaving overloads without redeclaring those operators
// or using its pre-C++11 exception spelling for placement declarations.
#undef __NO_THROW
#define __NO_THROW noexcept
#define __OPERATOR_NEW_DECLARED__
#define __PLACEMENT_NEW_INLINE
#define __PLACEMENT_VEC_NEW_INLINE

#endif  // SYMBIAN_NATIVE_CPP_COMPAT_H_
