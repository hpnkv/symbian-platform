#include <absl/base/nullability.h>
#include <e32base.h>
#include <hwrmvibra.h>

namespace {

void VibrateL() {
  auto* absl_nonnull vibra = CHWRMVibra::NewLC();
  vibra->StartVibraL(100, 25);
  User::After(150000);
  vibra->StopVibraL();
  CleanupStack::PopAndDestroy(vibra);
}

}  // namespace

int main() {
  TRAPD(error, VibrateL());
  return error;
}
