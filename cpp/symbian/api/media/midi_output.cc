// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "symbian/api/media/midi_output.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <new>

#include <absl/base/nullability.h>

#include "symbian/api/time/sleep.h"
#include "symbian/concurrency/bounded_channel.h"
#include "symbian/concurrency/worker_executor.h"
#include "symbian/native_status.h"

// The original inline E32 definitions require this declaration before the
// native header meets libc++'s exception declarations.
namespace std {
bool uncaught_exception();
}

#define __EXCEPTION__
#include <e32base.h>
#undef __EXCEPTION__
#include <midiclientutility.h>

namespace symbian::api::media {
namespace {

struct Note {
  int pitch = 0;
  int duration_ms = 0;
  int velocity = 0;
};

struct MidiState final : MMidiClientUtilityObserver {
  std::atomic<bool> stop{false};
  std::atomic<bool> available{false};
  std::atomic<int> error{KErrNone};
  symbian::concurrency::BoundedChannel<Note> notes{8};

  void MmcuoStateChanged(TMidiState, TMidiState,
                         const TTimeIntervalMicroSeconds&,
                         TInt result) override {
    if (result != KErrNone) {
      error.store(result);
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
      error.store(KErrNoMemory);
      return;
    }
    CActiveScheduler scheduler;
    CActiveScheduler* absl_nullable previous = CActiveScheduler::Current();
    CActiveScheduler::Install(&scheduler);
    CMidiClientUtility* absl_nullable midi = nullptr;
    TRAPD(open_error, midi = CMidiClientUtility::NewL(*this));
    if (open_error == KErrNone && midi != nullptr) {
      available.store(true);
      while (!stop.load() && error.load() == KErrNone) {
        TInt callback_error = KErrNone;
        for (int count = 0;
             count < 4 && CActiveScheduler::RunIfReady(callback_error,
                                                       CActive::EPriorityIdle);
             ++count) {
          if (callback_error != KErrNone) {
            error.store(callback_error);
            break;
          }
        }
        Note note;
        if (auto received = notes.TryRead(&note);
            received.ok() && *received && error.load() == KErrNone) {
          TRAPD(play_error, midi->PlayNoteL(0, note.pitch,
                                            TTimeIntervalMicroSeconds(
                                                note.duration_ms * 1000),
                                            note.velocity, 0));
          if (play_error != KErrNone) {
            error.store(play_error);
          }
        }
        symbian::api::time::SleepFor(std::chrono::milliseconds(20));
      }
    } else {
      error.store(open_error != KErrNone ? open_error : KErrGeneral);
    }
    available.store(false);
    delete midi;
    CActiveScheduler::Install(previous);
    delete cleanup;
  }
};

}  // namespace

struct MidiOutput::Impl {
  std::shared_ptr<MidiState> state = std::make_shared<MidiState>();
  symbian::concurrency::WorkerExecutor* absl_nullable worker = nullptr;
  absl::Status startup = absl::FailedPreconditionError("MIDI not started");
};

MidiOutput::MidiOutput() : impl_(new (std::nothrow) Impl) {}

MidiOutput::~MidiOutput() {
  if (impl_ == nullptr) {
    return;
  }
  impl_->state->stop.store(true);
  impl_->state->notes.Close();
  delete impl_->worker;
  delete impl_;
}

absl::Status MidiOutput::Start() {
  if (impl_ == nullptr) {
    return absl::ResourceExhaustedError("MIDI owner allocation failed");
  }
  if (impl_->worker != nullptr) {
    return impl_->startup;
  }
  impl_->worker = new (std::nothrow) symbian::concurrency::WorkerExecutor(1);
  if (impl_->worker == nullptr) {
    impl_->startup =
        absl::ResourceExhaustedError("MIDI worker allocation failed");
    return impl_->startup;
  }
  const std::shared_ptr<MidiState> state = impl_->state;
  impl_->startup = impl_->worker->Post([state] { state->Run(); });
  if (!impl_->startup.ok()) {
    delete impl_->worker;
    impl_->worker = nullptr;
  }
  return impl_->startup;
}

absl::Status MidiOutput::PlayNote(int note, int duration_ms, int velocity) {
  if (note < 0 || note > 127 || duration_ms <= 0 || duration_ms > 60000 ||
      velocity <= 0 || velocity > 127) {
    return absl::InvalidArgumentError(
        "Invalid MIDI note, duration or velocity");
  }
  if (const absl::Status ready = status(); !ready.ok()) {
    return ready;
  }
  return impl_->state->notes.TryWrite(
      Note{.pitch = note, .duration_ms = duration_ms, .velocity = velocity});
}

bool MidiOutput::available() const {
  return impl_ != nullptr && impl_->state->available.load();
}

absl::Status MidiOutput::status() const {
  if (impl_ == nullptr) {
    return absl::ResourceExhaustedError("MIDI owner allocation failed");
  }
  if (!impl_->startup.ok()) {
    return impl_->startup;
  }
  if (const int error = impl_->state->error.load(); error != KErrNone) {
    return symbian::StatusFromNativeError(error, "MIDI service");
  }
  return available() ? absl::OkStatus()
                     : absl::UnavailableError("MIDI service opening");
}

}  // namespace symbian::api::media
