#include <absl/base/nullability.h>
#include <centralrepository.h>
#include <sipsdkcrkeys.h>

#include "ui.h"

int RunFeature(void* absl_nullable) {
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

int main() {
  return classic_demo_ui::Show(
      _L("CENTRAL REPOSITORY"), _L("Reads a SIP repository key."),
      _L("Checks the timer is present."), _L("SIP TIMER VALUE READ"),
      &RunFeature, nullptr);
}
