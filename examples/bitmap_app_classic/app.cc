#include <absl/base/nullability.h>
#include <bitdev.h>
#include <bitstd.h>
#include <e32base.h>
#include <fbs.h>

namespace {

void DrawL() {
  auto* absl_nonnull bitmap = new (ELeave) CFbsBitmap;
  CleanupStack::PushL(bitmap);
  User::LeaveIfError(bitmap->Create(TSize(128, 48), EColor16MU));
  auto* absl_nonnull device = CFbsBitmapDevice::NewL(bitmap);
  CleanupStack::PushL(device);
  CFont* absl_nullable font = nullptr;
  User::LeaveIfError(
      device->GetNearestFontInTwips(font, TFontSpec(_L("Arial"), 180)));
  CFbsBitGc* absl_nullable context = nullptr;
  const TInt created = device->CreateContext(context);
  if (created != KErrNone) {
    device->ReleaseFont(font);
    User::Leave(created);
  }
  context->UseFont(font);
  context->SetPenColor(KRgbWhite);
  context->SetBrushColor(KRgbBlack);
  context->Clear();
  context->DrawText(_L("Native fonts"), TPoint(4, 24));
  context->DiscardFont();
  delete context;
  device->ReleaseFont(font);
  CleanupStack::PopAndDestroy(device);
  CleanupStack::PopAndDestroy(bitmap);
}

}  // namespace

int main() {
  const TInt connected = RFbsSession::Connect();
  if (connected != KErrNone) {
    return connected;
  }
  TRAPD(error, DrawL());
  RFbsSession::Disconnect();
  return error;
}
