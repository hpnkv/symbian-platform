#include <absl/base/nullability.h>
#include <e32base.h>
#include <hwrmvibra.h>

#include "ui.h"

namespace {

void VibrateL() {
  auto* absl_nonnull vibra = CHWRMVibra::NewLC();
  vibra->StartVibraL(100, 25);
  User::After(150000);
  vibra->StopVibraL();
  CleanupStack::PopAndDestroy(vibra);
}

}  // namespace

int RunFeature(void* absl_nullable) {
  TRAPD(error, VibrateL());
  return error;
}

int main() {
  return classic_demo_ui::Show(
      _L("VIBRATION MOTOR"), _L("Pulses the vibration motor."),
      _L("Stops it after 150 ms."), _L("VIBRATION PULSE SENT"), &RunFeature,
      nullptr);
}
