// SPDX-License-Identifier: Apache-2.0
// Shared EABI provider for ROMs exposing the error-preserving secure RNG API.
#include <absl/base/nullability.h>
#include <e32std.h>

#include "mbedtls/entropy.h"
#include "mbedtls/platform_util.h"
#include "veneer.h"

namespace {

bool InRom(std::uintptr_t address, std::size_t length) {
  if (address == 0 || length == 0 || address > UINT32_MAX - length) {
    return false;
  }
  // Check every byte before dereferencing native code. IsRomAddress uses the
  // OS's main/extension ROM bounds; unsupported/RAM wrappers fail closed.
  for (std::size_t i = 0; i < length; ++i) {
    TBool in_rom = EFalse;
    if (User::IsRomAddress(in_rom, reinterpret_cast<TAny*>(address + i)) !=
            KErrNone ||
        !in_rom) {
      return false;
    }
  }
  return true;
}

std::uintptr_t ResolveVeneer(TLibraryFunction absl_nullable function,
                             bool leaving) {
  const auto address = reinterpret_cast<std::uintptr_t>(function);
  const std::size_t count = leaving ? 6 : 4;
  const auto base = address & ~std::uintptr_t{1};
  if (!InRom(base, count * sizeof(std::uint16_t))) {
    return 0;
  }
  return symbian::entropy::SecureRandomVeneer(
      address, {reinterpret_cast<const std::uint16_t*>(base), count}, leaving);
}

}  // namespace

extern "C" int mbedtls_hardware_poll(void* absl_nullable data,
                                     unsigned char* absl_nullable output,
                                     size_t len, size_t* absl_nullable olen) {
  (void)data;
  if (olen == nullptr) {
    return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
  }
  *olen = 0;
  if (output == nullptr || len == 0 || len > 1024) {
    return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
  }
  RLibrary library;
  _LIT(KEuser, "euser.dll");
  TInt result = library.Load(KEuser);
  if (result == KErrNone) {
    // EABI exports 2503/2504 are Math::RandomL/Random(TDes8&). Random itself
    // discards KErrNotReady; calling its validated non-leaving veneer preserves
    // that error without requiring an unverified C++ leave/unwind boundary.
    const auto target = ResolveVeneer(library.Lookup(2504), false);
    if (target != 0 && target == ResolveVeneer(library.Lookup(2503), true) &&
        InRom(target, 2 * sizeof(std::uint32_t)) &&
        symbian::entropy::IsSecureRandomVeneer(
            {reinterpret_cast<const std::uint32_t*>(target), 2})) {
      using SecureRandom = TInt (*absl_nonnull)(TDes8* absl_nonnull);
      TPtr8 random(output, static_cast<TInt>(len), static_cast<TInt>(len));
      result = reinterpret_cast<SecureRandom>(target)(&random);
      if (result == KErrNone && random.Length() != static_cast<TInt>(len)) {
        result = KErrNotReady;
      }
    } else {
      result = KErrNotSupported;
    }
    library.Close();
  }
  if (result != KErrNone) {
    mbedtls_platform_zeroize(output, len);
    return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
  }
  *olen = len;
  return 0;
}
