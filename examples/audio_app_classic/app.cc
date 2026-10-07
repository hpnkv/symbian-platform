#include <absl/base/nullability.h>
#include <e32base.h>
#include <mda/common/audio.h>
#include <mdaaudiooutputstream.h>

#include "ui.h"

namespace {

class Audio final : public MMdaAudioOutputStreamCallback {
 public:
  ~Audio() { delete stream_; }

  void StartL() {
    // A quarter second of quiet 250 Hz, signed little-endian PCM16 at 8 kHz.
    for (TInt sample = 0; sample < 2000; ++sample) {
      const TInt value = sample % 32 < 16 ? 1000 : -1000;
      samples_.Append(static_cast<TUint8>(value & 255));
      samples_.Append(static_cast<TUint8>((value >> 8) & 255));
    }
    stream_ = CMdaAudioOutputStream::NewL(*this);
    User::LeaveIfError(stream_->KeepOpenAtEnd());
    settings_.iSampleRate = TMdaAudioDataSettings::ESampleRate8000Hz;
    settings_.iChannels = TMdaAudioDataSettings::EChannelsMono;
    stream_->Open(&settings_);
    CActiveScheduler::Start();
    User::LeaveIfError(error_);
  }

  void MaoscOpenComplete(TInt error) override {
    if (error == KErrNone) {
      stream_->SetVolume(stream_->MaxVolume() / 8);
      TRAP(error, stream_->WriteL(samples_));
    }
    if (error != KErrNone) {
      error_ = error;
      CActiveScheduler::Stop();
    }
  }

  // Preserve the original external observer reference contract.
  void MaoscBufferCopied(TInt error, const TDesC8&) override {
    if (error != KErrNone) {
      error_ = error;
      CActiveScheduler::Stop();
    } else {
      const TInt stopped = stream_->RequestStop();
      if (stopped != KErrNone) {
        error_ = stopped;
        CActiveScheduler::Stop();
      }
    }
  }

  void MaoscPlayComplete(TInt error) override {
    error_ = error == KErrUnderflow ? KErrNone : error;
    CActiveScheduler::Stop();
  }

 private:
  CMdaAudioOutputStream* absl_nullable stream_ = nullptr;
  TMdaAudioDataSettings settings_;
  TBuf8<4000> samples_;
  TInt error_ = KErrNone;
};

void PlayL() {
  CActiveScheduler scheduler;
  CActiveScheduler::Install(&scheduler);
  Audio audio;
  TRAPD(error, audio.StartL());
  CActiveScheduler::Install(nullptr);
  User::LeaveIfError(error);
}

}  // namespace

int RunFeature(void* absl_nullable) {
  TRAPD(error, PlayL());
  return error;
}

int main() {
  return classic_demo_ui::Show(_L("MDA AUDIO STREAM"),
                               _L("Generates a 250 Hz PCM tone."),
                               _L("Plays it with an MDA stream."),
                               _L("MDA TONE PLAYED"), &RunFeature, nullptr);
}
