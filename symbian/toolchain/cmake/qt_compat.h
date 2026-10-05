#ifndef SYMBIAN_QT_COMPAT_H_
#define SYMBIAN_QT_COMPAT_H_

#include <new>

#include <e32def.h>

// Original Qt headers expect the platform's new declarations. libc++ supplies
// those declarations; prevent the historical headers from redeclaring them.
#undef __NO_THROW
#define __NO_THROW noexcept
#define __SYMBIAN_STDCPP_SUPPORT__
#define __PLACEMENT_NEW_INLINE
#define __PLACEMENT_VEC_NEW_INLINE

#endif  // SYMBIAN_QT_COMPAT_H_
