#include <absl/base/nullability.h>
#include <bitdev.h>
#include <bitstd.h>
#include <e32base.h>
#include <fbs.h>

#include "ui.h"

namespace {

void DrawL(TUint32* absl_nonnull preview) {
  auto* absl_nonnull bitmap = new (ELeave) CFbsBitmap;
  CleanupStack::PushL(bitmap);
  User::LeaveIfError(bitmap->Create(TSize(128, 48), EColor16MU));
  auto* absl_nonnull device = CFbsBitmapDevice::NewL(bitmap);
  CleanupStack::PushL(device);
  CFont* absl_nullable font = nullptr;
  User::LeaveIfError(
      device->GetNearestFontInTwips(font, TFontSpec(_L("Arial"), 180)));
  CFbsBitGc* absl_nullable context = nullptr;
  if (const TInt created = device->CreateContext(context);
      created != KErrNone) {
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
  for (TInt y = 0; y < 6; ++y) {
    for (TInt x = 0; x < 16; ++x) {
      TUint32 color = 0;
      for (TInt offset_y = 0; offset_y < 8 && color == 0; offset_y += 2) {
        for (TInt offset_x = 0; offset_x < 8; offset_x += 2) {
          TRgb pixel;
          bitmap->GetPixel(pixel, TPoint(x * 8 + offset_x, y * 8 + offset_y));
          if (pixel.Red() > 128) {
            color = 0x00ffffff;
            break;
          }
        }
      }
      preview[y * 16 + x] = color;
    }
  }
  CleanupStack::PopAndDestroy(device);
  CleanupStack::PopAndDestroy(bitmap);
}

}  // namespace

int RunFeature(TUint32* absl_nonnull preview) {
  if (const TInt connected = RFbsSession::Connect(); connected != KErrNone) {
    return connected;
  }
  TRAPD(error, DrawL(preview));
  RFbsSession::Disconnect();
  return error;
}

int InvokeFeature(void* absl_nullable context) {
  if (context == nullptr) {
    return KErrArgument;
  }
  return RunFeature(static_cast<TUint32*>(context));
}

int main() {
  TUint32 preview[96] = {};
  return classic_demo_ui::Show(_L("BITMAP GDI"), _L("Creates an RGB bitmap."),
                               _L("Draws into it with GDI."),
                               _L("RGB BITMAP DRAWN"), &InvokeFeature, preview,
                               preview, 16, 6);
}
