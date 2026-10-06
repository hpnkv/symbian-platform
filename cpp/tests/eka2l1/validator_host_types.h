#ifndef SYMBIAN_TESTS_VALIDATOR_HOST_TYPES_H_
#define SYMBIAN_TESTS_VALIDATOR_HOST_TYPES_H_

#include <cstddef>
#include <cstdint>

#include <absl/base/nullability.h>

#include "checksum.h"
#include "checksum_host_types.h"

// Host declarations for compiling the original f32image.h validation methods.
// Suppress unrelated SDK APIs, not the validator or its image structures.
// Values/layouts come from the pinned e32const/err/ldr_private/uid headers.
// This is an oracle adapter, not an SDK or target ABI implementation.
#define __E32CMN_H__
#define __E32LDR_H__
#define __E32LDR_PRIVATE_H__
#define __E32UID_H__
#define _FOFF(type, field) static_cast<uint32_t>(offsetof(type, field))

using TAny = void;
using TInt = int32_t;
using TInt16 = int16_t;
using TInt32 = int32_t;
using TUint = uint32_t;
using TUint8 = uint8_t;
using TUint16 = uint16_t;
using TUint32 = uint32_t;
using TBool = int32_t;
using TProcessPriority = int32_t;

struct TVersion {
  int8_t major;
  int8_t minor;
  int16_t build;
};

struct SSecurityInfo {
  uint32_t secure_id;
  uint32_t vendor_id;
  uint32_t capabilities[2];
};

struct TUidType {
  TUid uids[3];
};

struct TCheckedUid {
  explicit TCheckedUid(const TUidType& value)
      : type(value),
        check(static_cast<uint32_t>(
            (checkSum(reinterpret_cast<const uint8_t*>(&type) + 1) << 16) |
            checkSum(&type))) {}

  TUidType type;
  uint32_t check;
};

struct Mem {
  static void Crc32(TUint32* absl_nonnull value, const TAny* absl_nonnull data,
                    TInt length) {
    unsigned long crc = *value;
    ::Crc32(crc, data, static_cast<size_t>(length));
    *value = static_cast<TUint32>(crc);
  }
};

constexpr TInt KErrNone = 0;
constexpr TInt KErrNoMemory = -4;
constexpr TInt KErrNotSupported = -5;
constexpr TInt KErrCorrupt = -20;
constexpr TUint EFpTypeNone = 0;
constexpr TUint EFpTypeVFPv2 = 1;
constexpr TUint EFpTypeVFPv3 = 2;
constexpr TUint EFpTypeVFPv3D16 = 3;
constexpr TUint KDynamicLibraryUidValue = 0x10000079;
constexpr TUint KExecutableImageUidValue = 0x1000007a;
constexpr TUint KFormatNotCompressed = 0;
constexpr TUint KUidCompressionDeflate = 0x101f7afc;
constexpr TUint KUidCompressionBytePair = 0x102822aa;
constexpr TUint KTextRelocType = 0x1000;
constexpr TUint KDataRelocType = 0x2000;
constexpr TUint KInferredRelocType = 0x3000;

static_assert(sizeof(TVersion) == 4);
static_assert(sizeof(SSecurityInfo) == 16);
static_assert(sizeof(TUidType) == 12);
static_assert(sizeof(TCheckedUid) == 16);

#endif  // SYMBIAN_TESTS_VALIDATOR_HOST_TYPES_H_
