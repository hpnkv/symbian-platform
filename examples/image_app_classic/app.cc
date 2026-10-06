#include <absl/base/nullability.h>
#include <e32base.h>
#include <f32file.h>
#include <fbs.h>
#include <imageconversion.h>

namespace {

// An owned one-pixel RGB PNG; the OS supplies the real image decoder plugin.
constexpr TUint8 kPng[] = {
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
    0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
    0x08, 0x02, 0x00, 0x00, 0x00, 0x90, 0x77, 0x53, 0xde, 0x00, 0x00, 0x00,
    0x0c, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9c, 0x63, 0xf8, 0xcf, 0xc0, 0x00,
    0x00, 0x03, 0x01, 0x01, 0x00, 0xc9, 0xfe, 0x92, 0xef, 0x00, 0x00, 0x00,
    0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82};

class Decode final : public CActive {
 public:
  Decode(CImageDecoder* absl_nonnull decoder, CFbsBitmap* absl_nonnull bitmap)
      : CActive(EPriorityStandard), decoder_(decoder) {
    CActiveScheduler::Add(this);
    decoder_->Convert(&iStatus, *bitmap);
    SetActive();
  }

  ~Decode() override { Cancel(); }

  TInt result() const { return iStatus.Int(); }

 private:
  void RunL() override { CActiveScheduler::Stop(); }

  void DoCancel() override { decoder_->Cancel(); }

  CImageDecoder* absl_nonnull decoder_;
};

void LoadL() {
  RFs files;
  User::LeaveIfError(files.Connect());
  CleanupClosePushL(files);
  const TPtrC8 png(kPng, sizeof(kPng));
  auto* absl_nonnull decoder = CImageDecoder::DataNewL(files, png);
  CleanupStack::PushL(decoder);
  auto* absl_nonnull bitmap = new (ELeave) CFbsBitmap;
  CleanupStack::PushL(bitmap);
  User::LeaveIfError(
      bitmap->Create(decoder->FrameInfo().iOverallSizeInPixels, EColor16M));
  auto* absl_nonnull request = new (ELeave) Decode(decoder, bitmap);
  CleanupStack::PushL(request);
  CActiveScheduler::Start();
  User::LeaveIfError(request->result());
  TRgb pixel;
  bitmap->GetPixel(pixel, TPoint(0, 0));
  if (pixel.Red() != 255 || pixel.Green() != 0 || pixel.Blue() != 0) {
    User::Leave(KErrCorrupt);
  }
  CleanupStack::PopAndDestroy(request);
  CleanupStack::PopAndDestroy(bitmap);
  CleanupStack::PopAndDestroy(decoder);
  CleanupStack::PopAndDestroy(&files);
}

}  // namespace

int main() {
  const TInt connected = RFbsSession::Connect();
  if (connected != KErrNone) {
    return connected;
  }
  CActiveScheduler scheduler;
  CActiveScheduler::Install(&scheduler);
  TRAPD(error, LoadL());
  RFbsSession::Disconnect();
  CActiveScheduler::Install(nullptr);
  return error;
}
