// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "symbian/api/media/game_feedback.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>

#include <absl/base/nullability.h>

#include "symbian/concurrency/worker_executor.h"
#include "symbian/native_status.h"
#include "symbian/api/time/monotonic_clock.h"
#include "symbian/api/time/sleep.h"

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

namespace symbian::api::media {
namespace {

constexpr int kVibrationPending = -10000;
constexpr std::uint64_t kVibrationDeadlineMs = 2000;
constexpr int kVibrationQueued = 0;
constexpr int kVibrationWorkerStarted = 1;
constexpr int kVibrationClientOpening = 2;
constexpr int kVibrationPulseStarting = 3;
constexpr int kVibrationClientClosing = 4;

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
  TRAPD(result, PulseVibrationL(stage));
  CActiveScheduler::Install(previous);
  delete cleanup;
  return result;
}

// The firmware MIDI server may block during client construction. Keep its
// active scheduler and all client calls on an independent worker thread.
struct MidiLoopState final : MMidiClientUtilityObserver {
  std::atomic<bool> stop{false};
  std::atomic<bool> enabled{true};
  std::atomic<bool> available{false};
  std::atomic<bool> failed{false};

  void MmcuoStateChanged(TMidiState, TMidiState,
                         const TTimeIntervalMicroSeconds&,
                         TInt error) override {
    if (error != KErrNone) {
      failed.store(true);
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

  void Run() {
    CTrapCleanup* absl_nullable cleanup = CTrapCleanup::New();
    if (cleanup == nullptr) {
      return;
    }
    CActiveScheduler scheduler;
    CActiveScheduler* absl_nullable previous = CActiveScheduler::Current();
    CActiveScheduler::Install(&scheduler);
    CMidiClientUtility* absl_nullable midi = nullptr;
    TRAPD(open_error, midi = CMidiClientUtility::NewL(*this));
    if (open_error == KErrNone && midi != nullptr) {
      available.store(true);
      constexpr std::int32_t kNotes[] = {60, 64, 67, 72, 67, 64, 62, 67};
      std::size_t note_index = 0;
      std::uint64_t next_note_ms = 0;
      while (!stop.load() && !failed.load()) {
        TInt callback_error = KErrNone;
        for (int count = 0; count < 4 && CActiveScheduler::RunIfReady(
                                             callback_error,
                                             CActive::EPriorityIdle);
             ++count) {
          if (callback_error != KErrNone) {
            failed.store(true);
            break;
          }
        }
        const std::uint64_t now = static_cast<std::uint64_t>(
            symbian::api::time::MonotonicClock::NowNanoseconds() / 1000000);
        if (enabled.load() && now >= next_note_ms && !failed.load()) {
          const std::int32_t note = kNotes[note_index++ % 8];
          TRAPD(play_error, midi->PlayNoteL(
                                0, note, TTimeIntervalMicroSeconds(170000),
                                52, 0));
          if (play_error != KErrNone) {
            failed.store(true);
          }
          next_note_ms = now + 220;
        }
        symbian::api::time::SleepFor(std::chrono::milliseconds(20));
      }
    }
    available.store(false);
    delete midi;
    CActiveScheduler::Install(previous);
    delete cleanup;
  }
};

}  // namespace

struct GameFeedback::Impl final {
  std::shared_ptr<MidiLoopState> audio = std::make_shared<MidiLoopState>();
  symbian::concurrency::WorkerExecutor* absl_nullable audio_worker = nullptr;
  symbian::concurrency::WorkerExecutor* absl_nullable vibration_worker =
      nullptr;
  std::shared_ptr<std::atomic<int>> vibration_result =
      std::make_shared<std::atomic<int>>(KErrNone);
  std::shared_ptr<std::atomic<int>> vibration_stage =
      std::make_shared<std::atomic<int>>(kVibrationQueued);
  std::uint64_t vibration_started_ms = 0;
  std::uint64_t last_hit_ms = 0;
  bool started = false;
  bool vibration_failed = false;
  absl::Status vibration_status = absl::OkStatus();
};

GameFeedback::GameFeedback() : impl_(new (std::nothrow) Impl) {}

GameFeedback::~GameFeedback() {
  if (impl_ == nullptr) {
    return;
  }
  impl_->audio->stop.store(true);
  delete impl_->audio_worker;
  delete impl_->vibration_worker;
  delete impl_;
}

void GameFeedback::Start() {
  if (impl_ == nullptr || impl_->started) {
    return;
  }
  impl_->started = true;
  impl_->audio_worker =
      new (std::nothrow) symbian::concurrency::WorkerExecutor(1);
  if (impl_->audio_worker != nullptr) {
    std::shared_ptr<MidiLoopState> audio = impl_->audio;
    impl_->audio_worker->Post([audio] { audio->Run(); }).IgnoreError();
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
    impl_->audio->enabled.store(enabled);
  }
}

void GameFeedback::Pump(std::uint64_t elapsed_ms) {
  if (impl_ == nullptr) {
    return;
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
}

void GameFeedback::Hit(std::uint64_t elapsed_ms) {
  if (impl_ == nullptr || impl_->vibration_worker == nullptr ||
      impl_->vibration_failed ||
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
  return impl_ != nullptr && impl_->audio->available.load();
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
