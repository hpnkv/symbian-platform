// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "symbian/api/media/game_feedback.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>

#include <absl/base/nullability.h>

#include "symbian/concurrency/worker_executor.h"
#include "symbian/native_status.h"

// Original e32base.inl uses this legacy function, while e32cmn.h's own
// declaration of std::terminate conflicts with the SDK's libc++ headers.
namespace std {
bool uncaught_exception();
}

#define __EXCEPTION__
#include <e32base.h>
#undef __EXCEPTION__
#include <hwrmvibra.h>
#include <midiclientutility.h>
#include <touchlogicalfeedback.h>

namespace symbian::api::media {
namespace {

constexpr int kVibrationPending = -10000;
constexpr std::uint64_t kVibrationDeadlineMs = 2000;
constexpr int kVibrationQueued = 0;
constexpr int kVibrationWorkerStarted = 1;
constexpr int kVibrationClientOpening = 2;
constexpr int kVibrationPulseStarting = 3;
constexpr int kVibrationClientClosing = 4;
constexpr int kTouchLibraryLoading = 10;
constexpr int kTouchClientOpening = 11;
constexpr int kTouchPulseStarting = 12;
constexpr int kTouchClientClosing = 13;

// RTactileFeedback is an RSessionBase followed by its owned server thread.
// Its firmware constructor has no body; the imported methods use this layout.
class TactileSession final : public RSessionBase {
 public:
  void* absl_nullable server_thread = nullptr;
};

int PulseTouchFeedback(std::atomic<int>* absl_nonnull stage) {
  // Starting this optional server can recursively start the haptics stack.
  // Use the system's already running feedback service when available.
  TFindServer server(_L("TactileFeedbackServer"));
  TFullName server_name;
  if (server.Next(server_name) != KErrNone) {
    return KErrNotFound;
  }
  stage->store(kTouchLibraryLoading);
  RLibrary library;
  const int loaded = library.Load(_L("tactilefeedbackresolver.dll"));
  if (loaded != KErrNone) {
    return loaded;
  }
  using Connect = TInt (*absl_nullable)(TactileSession* absl_nonnull);
  using Play = void (*absl_nullable)(TactileSession* absl_nonnull,
                                     TTouchLogicalFeedback, TBool, TBool);
  using Close = void (*absl_nullable)(TactileSession* absl_nonnull);
  const auto connect = reinterpret_cast<Connect>(library.Lookup(22));
  const auto play = reinterpret_cast<Play>(library.Lookup(11));
  const auto close = reinterpret_cast<Close>(library.Lookup(21));
  if (connect == nullptr || play == nullptr || close == nullptr) {
    library.Close();
    return KErrNotSupported;
  }
  TactileSession session{};
  stage->store(kTouchClientOpening);
  const int error = connect(&session);
  if (error == KErrNone) {
    stage->store(kTouchPulseStarting);
    // The firmware server chooses its light haptic profile; audio is disabled.
    play(&session, ETouchFeedbackSensitive, ETrue, EFalse);
  }
  stage->store(kTouchClientClosing);
  close(&session);
  library.Close();
  return error;
}

const char* absl_nonnull VibrationTimeoutMessage(int stage) {
  switch (stage) {
    case kVibrationQueued:
      return "HWRM worker did not start";
    case kVibrationWorkerStarted:
      return "HWRM thread setup timed out";
    case kVibrationClientOpening:
      return "HWRM client open timed out";
    case kVibrationPulseStarting:
      return "HWRM pulse timed out";
    case kVibrationClientClosing:
      return "HWRM client close timed out";
    case kTouchLibraryLoading:
      return "tactile resolver load timed out";
    case kTouchClientOpening:
      return "tactile resolver connect timed out";
    case kTouchPulseStarting:
      return "tactile resolver pulse timed out";
    case kTouchClientClosing:
      return "tactile resolver close timed out";
    default:
      return "HWRM vibration timed out";
  }
}

void PulseVibrationL(std::atomic<int>* absl_nonnull stage) {
  stage->store(kVibrationClientOpening);
  CHWRMVibra* absl_nonnull vibra = CHWRMVibra::NewLC();
  // Use the device's supported default intensity. A custom intensity is not
  // supported by every motor even when vibration itself is available.
  stage->store(kVibrationPulseStarting);
  vibra->StartVibraL(60);
  stage->store(kVibrationClientClosing);
  CleanupStack::PopAndDestroy(vibra);
}

int PulseVibration(std::atomic<int>* absl_nonnull stage) {
  stage->store(kVibrationWorkerStarted);
  CTrapCleanup* absl_nullable cleanup = CTrapCleanup::New();
  if (cleanup == nullptr) {
    return KErrNoMemory;
  }
  CActiveScheduler scheduler;
  CActiveScheduler* absl_nullable previous = CActiveScheduler::Current();
  CActiveScheduler::Install(&scheduler);
  int result = PulseTouchFeedback(stage);
  if (result != KErrNone) {
    TRAPD(error, PulseVibrationL(stage));
    result = error;
  }
  CActiveScheduler::Install(previous);
  delete cleanup;
  return result;
}

}  // namespace

struct GameFeedback::Impl final : MMidiClientUtilityObserver {
  CActiveScheduler scheduler;
  CActiveScheduler* absl_nullable previous = nullptr;
  CMidiClientUtility* absl_nullable midi = nullptr;
  symbian::concurrency::WorkerExecutor* absl_nullable vibration_worker =
      nullptr;
  std::shared_ptr<std::atomic<int>> vibration_result =
      std::make_shared<std::atomic<int>>(KErrNone);
  std::shared_ptr<std::atomic<int>> vibration_stage =
      std::make_shared<std::atomic<int>>(kVibrationQueued);
  std::uint64_t vibration_started_ms = 0;
  std::uint64_t next_note_ms = 0;
  std::uint64_t last_hit_ms = 0;
  std::size_t note_index = 0;
  bool installed = false;
  bool midi_failed = false;
  bool vibration_failed = false;
  absl::Status vibration_status = absl::OkStatus();
  bool music_enabled = true;

  void MmcuoStateChanged(TMidiState, TMidiState,
                         const TTimeIntervalMicroSeconds&,
                         TInt error) override {
    if (error != KErrNone) {
      midi_failed = true;
    }
  }

  void MmcuoTempoChanged(TInt) override {}

  void MmcuoVolumeChanged(TInt, TReal32) override {}

  void MmcuoMuteChanged(TInt, TBool) override {}

  void MmcuoSyncUpdate(const TTimeIntervalMicroSeconds&, TInt64) override {}

  void MmcuoMetaDataEntryFound(const TInt,
                               const TTimeIntervalMicroSeconds&) override {}

  void MmcuoMipMessageReceived(const RArray<TMipMessageEntry>&) override {}

  void MmcuoPolyphonyChanged(TInt) override {}

  void MmcuoInstrumentChanged(TInt, TInt, TInt) override {}
};

GameFeedback::GameFeedback() : impl_(new (std::nothrow) Impl) {}

GameFeedback::~GameFeedback() {
  if (impl_ == nullptr) {
    return;
  }
  delete impl_->midi;
  delete impl_->vibration_worker;
  if (impl_->installed) {
    CActiveScheduler::Install(impl_->previous);
  }
  delete impl_;
}

void GameFeedback::Start() {
  if (impl_ == nullptr) {
    return;
  }
  if (CActiveScheduler::Current() == nullptr) {
    impl_->previous = CActiveScheduler::Current();
    CActiveScheduler::Install(&impl_->scheduler);
    impl_->installed = true;
  }
  TRAPD(midi_error, impl_->midi = CMidiClientUtility::NewL(*impl_));
  if (midi_error != KErrNone) {
    impl_->midi = nullptr;
  }
  impl_->vibration_worker =
      new (std::nothrow) symbian::concurrency::WorkerExecutor(1);
  if (impl_->vibration_worker == nullptr) {
    impl_->vibration_failed = true;
    impl_->vibration_status =
        absl::ResourceExhaustedError("vibration worker allocation failed");
  }
}

void GameFeedback::SetMusicEnabled(bool enabled) {
  if (impl_ != nullptr) {
    impl_->music_enabled = enabled;
  }
}

void GameFeedback::Pump(std::uint64_t elapsed_ms) {
  if (impl_ == nullptr) {
    return;
  }
  if (impl_->installed) {
    TInt error = KErrNone;
    // A callback may immediately complete another request. Keep media work
    // bounded so input and presentation always get a turn.
    for (int count = 0; count < 4 && CActiveScheduler::RunIfReady(
                                         error, CActive::EPriorityIdle);
         ++count) {
      if (error != KErrNone) {
        break;
      }
    }
  }
  if (impl_->midi_failed) {
    delete impl_->midi;
    impl_->midi = nullptr;
    impl_->midi_failed = false;
  }
  if (!impl_->vibration_failed && impl_->vibration_worker != nullptr) {
    const int vibration_result = impl_->vibration_result->load();
    if (vibration_result != KErrNone && vibration_result != kVibrationPending) {
      impl_->vibration_failed = true;
      impl_->vibration_status =
          symbian::StatusFromNativeError(vibration_result, "HWRM vibration");
      impl_->vibration_worker->Close();
    } else if (vibration_result == kVibrationPending &&
               elapsed_ms - impl_->vibration_started_ms >
                   kVibrationDeadlineMs) {
      impl_->vibration_failed = true;
      impl_->vibration_status = absl::DeadlineExceededError(
          VibrationTimeoutMessage(impl_->vibration_stage->load()));
      impl_->vibration_worker->Close();
    }
  }
  if (impl_->midi == nullptr || !impl_->music_enabled ||
      elapsed_ms < impl_->next_note_ms) {
    return;
  }
  constexpr std::int32_t kNotes[] = {60, 64, 67, 72, 67, 64, 62, 67};
  const std::int32_t note = kNotes[impl_->note_index++ % 8];
  TRAPD(error, impl_->midi->PlayNoteL(
                   0, note, TTimeIntervalMicroSeconds(170000), 52, 0));
  if (error != KErrNone) {
    delete impl_->midi;
    impl_->midi = nullptr;
  }
  impl_->next_note_ms = elapsed_ms + 220;
}

void GameFeedback::Hit(std::uint64_t elapsed_ms) {
  if (impl_ == nullptr || impl_->vibration_failed ||
      elapsed_ms - impl_->last_hit_ms < 120) {
    return;
  }
  if (impl_->vibration_result->load() == kVibrationPending) {
    return;
  }
  impl_->vibration_result->store(kVibrationPending);
  impl_->vibration_stage->store(kVibrationQueued);
  auto result = impl_->vibration_result;
  auto stage = impl_->vibration_stage;
  const absl::Status posted = impl_->vibration_worker->Post(
      [result, stage]() { result->store(PulseVibration(stage.get())); });
  if (!posted.ok()) {
    impl_->vibration_result->store(KErrGeneral);
    impl_->vibration_failed = true;
    impl_->vibration_status = posted;
    return;
  }
  impl_->vibration_started_ms = elapsed_ms;
  impl_->last_hit_ms = elapsed_ms;
}

bool GameFeedback::midi_available() const {
  return impl_ != nullptr && impl_->midi != nullptr;
}

bool GameFeedback::vibration_available() const {
  return impl_ != nullptr && !impl_->vibration_failed &&
         impl_->vibration_worker != nullptr &&
         impl_->vibration_result->load() == KErrNone;
}

absl::Status GameFeedback::vibration_status() const {
  return impl_ == nullptr
             ? absl::ResourceExhaustedError("game feedback unavailable")
             : impl_->vibration_status;
}

}  // namespace symbian::api::media
