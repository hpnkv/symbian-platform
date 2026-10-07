// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

// The C++ ABI requires this trap for an invalid pure virtual dispatch.
// Keeping it in the guest runtime also gives vtables a local function address.
extern "C" [[noreturn]] void __cxa_pure_virtual() {
  __builtin_trap();
}
