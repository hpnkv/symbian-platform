// Copyright 2026 Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0 (the "License");
// http://www.apache.org/licenses/LICENSE-2.0
//
// OpenC's stdio.h includes stdarg_e.h before stdarg.h. Its original header
// replaces Clang's va_start/va_arg with pointer arithmetic for an older ARM
// compiler. On the SDK's ARM EABI Clang, the builtin va_list is a one-pointer
// structure. Preserve Clang's builtin varargs operations and the C parameter
// representation required by the imported OpenC v*printf functions.

#ifndef SYMBIAN_RUNTIME_STDARG_E_H_
#define SYMBIAN_RUNTIME_STDARG_E_H_

typedef __builtin_va_list __e32_va_list;

#endif  // SYMBIAN_RUNTIME_STDARG_E_H_
