// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include <e32std.h>

// The Open C ABI's immediate-exit path does not run C/C++ finalizers.
extern "C" [[noreturn]] void _exit(int code) {
  User::Exit(code);
  __builtin_unreachable();
}
