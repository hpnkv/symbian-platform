// SPDX-License-Identifier: Apache-2.0
#ifndef SYMBIAN_TLS_NATIVE_STDARG_E_H_
#define SYMBIAN_TLS_NATIVE_STDARG_E_H_
// The selected SDK's OpenC stdio declarations include the old ESTLIB header.
// Native local C functions use Clang's actual AAPCS variadic ABI, not RVCT's
// char-pointer macros. Do not pass this type into frozen system vprintf APIs.
#include <stdarg.h>
typedef __builtin_va_list __e32_va_list;
#endif
