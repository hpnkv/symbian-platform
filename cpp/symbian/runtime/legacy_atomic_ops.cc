// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "legacy_atomic_ops.h"

#include <absl/base/nullability.h>
#include <e32std.h>

namespace {

volatile TInt g_lock = 0;

TInt ExchangeLock(volatile TInt* absl_nonnull pointer, TInt desired) {
  TInt previous;
  asm volatile("swp %0, %2, [%1]"
               : "=&r"(previous)
               : "r"(pointer), "r"(desired)
               : "memory");
  return previous;
}

class Guard {
 public:
  Guard() {
    while (ExchangeLock(&g_lock, 1) != 0) {
      User::After(TTimeIntervalMicroSeconds32(0));
    }
  }

  ~Guard() {
    asm volatile("" ::: "memory");
    g_lock = 0;
  }

  Guard(const Guard&) = delete;
  Guard& operator=(const Guard&) = delete;
};

}  // namespace

TUint8 LegacyAtomicLoad8(const volatile TAny* absl_nonnull pointer) {
  Guard guard;
  return *static_cast<const volatile TUint8*>(pointer);
}

TUint8 LegacyAtomicStore8(volatile TAny* absl_nonnull pointer, TUint8 value) {
  Guard guard;
  *static_cast<volatile TUint8*>(pointer) = value;
  return value;
}

TUint8 LegacyAtomicExchange8(volatile TAny* absl_nonnull pointer,
                             TUint8 value) {
  Guard guard;
  auto* absl_nonnull typed = static_cast<volatile TUint8*>(pointer);
  const TUint8 old = *typed;
  *typed = value;
  return old;
}

TBool LegacyAtomicCas8(volatile TAny* absl_nonnull pointer,
                       TUint8* absl_nonnull expected, TUint8 desired) {
  Guard guard;
  auto* absl_nonnull typed = static_cast<volatile TUint8*>(pointer);
  if (const TUint8 old = *typed; old != *expected) {
    *expected = old;
    return EFalse;
  }
  *typed = desired;
  return ETrue;
}

TUint32 LegacyAtomicLoad32(const volatile TAny* absl_nonnull pointer) {
  Guard guard;
  return *static_cast<const volatile TUint32*>(pointer);
}

TUint32 LegacyAtomicStore32(volatile TAny* absl_nonnull pointer,
                            TUint32 value) {
  Guard guard;
  *static_cast<volatile TUint32*>(pointer) = value;
  return value;
}

TUint32 LegacyAtomicExchange32(volatile TAny* absl_nonnull pointer,
                               TUint32 value) {
  Guard guard;
  auto* absl_nonnull typed = static_cast<volatile TUint32*>(pointer);
  const TUint32 old = *typed;
  *typed = value;
  return old;
}

TBool LegacyAtomicCas32(volatile TAny* absl_nonnull pointer,
                        TUint32* absl_nonnull expected, TUint32 desired) {
  Guard guard;
  auto* absl_nonnull typed = static_cast<volatile TUint32*>(pointer);
  if (const TUint32 old = *typed; old != *expected) {
    *expected = old;
    return EFalse;
  }
  *typed = desired;
  return ETrue;
}

TUint32 LegacyAtomicAdd32(volatile TAny* absl_nonnull pointer, TUint32 value) {
  Guard guard;
  auto* absl_nonnull typed = static_cast<volatile TUint32*>(pointer);
  const TUint32 old = *typed;
  *typed = old + value;
  return old;
}

TUint32 LegacyAtomicAnd32(volatile TAny* absl_nonnull pointer, TUint32 value) {
  Guard guard;
  auto* absl_nonnull typed = static_cast<volatile TUint32*>(pointer);
  const TUint32 old = *typed;
  *typed = old & value;
  return old;
}

TUint32 LegacyAtomicOr32(volatile TAny* absl_nonnull pointer, TUint32 value) {
  Guard guard;
  auto* absl_nonnull typed = static_cast<volatile TUint32*>(pointer);
  const TUint32 old = *typed;
  *typed = old | value;
  return old;
}

void LegacyAtomicBarrier() {
  Guard guard;
}
