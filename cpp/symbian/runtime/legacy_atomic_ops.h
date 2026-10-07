#ifndef SYMBIAN_RUNTIME_LEGACY_ATOMIC_OPS_H_
#define SYMBIAN_RUNTIME_LEGACY_ATOMIC_OPS_H_

#include <absl/base/nullability.h>
#include <e32std.h>

// E71-era EUSER lacks the later __e32_atomic_* functions. These helpers
// serialize the bounded runtime's compiler-ABI atomics with ARM SWP.
TUint8 LegacyAtomicLoad8(const volatile TAny* absl_nonnull pointer);
TUint8 LegacyAtomicStore8(volatile TAny* absl_nonnull pointer, TUint8 value);
TUint8 LegacyAtomicExchange8(volatile TAny* absl_nonnull pointer, TUint8 value);
TBool LegacyAtomicCas8(volatile TAny* absl_nonnull pointer,
                       TUint8* absl_nonnull expected, TUint8 desired);
TUint32 LegacyAtomicLoad32(const volatile TAny* absl_nonnull pointer);
TUint32 LegacyAtomicStore32(volatile TAny* absl_nonnull pointer, TUint32 value);
TUint32 LegacyAtomicExchange32(volatile TAny* absl_nonnull pointer,
                               TUint32 value);
TBool LegacyAtomicCas32(volatile TAny* absl_nonnull pointer,
                        TUint32* absl_nonnull expected, TUint32 desired);
TUint32 LegacyAtomicAdd32(volatile TAny* absl_nonnull pointer, TUint32 value);
TUint32 LegacyAtomicAnd32(volatile TAny* absl_nonnull pointer, TUint32 value);
TUint32 LegacyAtomicOr32(volatile TAny* absl_nonnull pointer, TUint32 value);
void LegacyAtomicBarrier();

#endif  // SYMBIAN_RUNTIME_LEGACY_ATOMIC_OPS_H_
