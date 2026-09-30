#ifndef SYMBIAN_TESTS_CHECKSUM_HOST_TYPES_H_
#define SYMBIAN_TESTS_CHECKSUM_HOST_TYPES_H_

#include <cstdint>

// The historical checksum.cpp only needs these two declarations from its
// target header. Keep its algorithm unchanged and avoid importing OS headers.
// In particular TUid remains four bytes on this LP64 host.
#define E32IMAGEDEFS_H

struct TUid {
  int32_t value;
};

constexpr int KMaxCheckedUid = 3;
static_assert(sizeof(TUid) == 4);

#endif  // SYMBIAN_TESTS_CHECKSUM_HOST_TYPES_H_
