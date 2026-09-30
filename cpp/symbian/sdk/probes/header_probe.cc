#include <e32std.h>

static_assert(sizeof(TInt) == 4);
static_assert(sizeof(TUint) == 4);
static_assert(sizeof(TInt64) == 8);
static_assert(sizeof(TUid) == 4);
static_assert(sizeof(TRequestStatus) == 8);
static_assert(sizeof(TTimeIntervalMicroSeconds32) == 4);
static_assert(sizeof(TDesC16) == 4);
static_assert(sizeof(TDes16) == 8);
static_assert(sizeof(TPtrC16) == 8);
static_assert(sizeof(TPtr16) == 12);

extern "C" void HeaderExitProbe(TInt reason) {
  User::Exit(reason);
}
