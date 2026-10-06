#include <absl/base/nullability.h>
#include <centralrepository.h>
#include <sipsdkcrkeys.h>

int main() {
  CRepository* absl_nullable repository = nullptr;
  TRAPD(error, repository = CRepository::NewL(KCRUidSIP));
  if (error != KErrNone) {
    return 1;
  }
  TInt value = 0;
  const TInt read_error = repository->Get(KSIPTransactionTimerT1, value);
  delete repository;
  return read_error == KErrNone && value > 0 ? 0 : 2;
}
