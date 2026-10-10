// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "symbian/api/media/vibration.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <new>

#include <absl/base/nullability.h>

#include "symbian/api/time/monotonic_clock.h"
#include "symbian/concurrency/worker_executor.h"
#include "symbian/native_status.h"

namespace std {
bool uncaught_exception();
}

#define __EXCEPTION__
#include <e32base.h>
#undef __EXCEPTION__
#include <hwrmvibra.h>

namespace symbian::api::media {
namespace {

constexpr int kPending = -10000;
constexpr std::int64_t kServiceDeadlineNs = 2000000000;
constexpr int kQueued = 0;
constexpr int kWorkerStarted = 1;
constexpr int kClientOpening = 2;
constexpr int kPulseStarting = 3;
constexpr int kClientClosing = 4;

const char* absl_nonnull TimeoutMessage(int stage) {
  switch (stage) {
    case kQueued:
      return "HWRM worker did not start";
    case kWorkerStarted:
      return "HWRM thread setup timed out";
    case kClientOpening:
      return "HWRM client open timed out";
    case kPulseStarting:
      return "HWRM pulse timed out";
    case kClientClosing:
      return "HWRM client close timed out";
    default:
      return "HWRM vibration timed out";
  }
}

void PulseL(int duration_ms, std::atomic<int>* absl_nonnull stage) {
  stage->store(kClientOpening);
  CHWRMVibra* absl_nonnull vibra = CHWRMVibra::NewLC();
  stage->store(kPulseStarting);
  vibra->StartVibraL(duration_ms);
  stage->store(kClientClosing);
  CleanupStack::PopAndDestroy(vibra);
}

int RunPulse(int duration_ms, std::atomic<int>* absl_nonnull stage) {
  stage->store(kWorkerStarted);
  CTrapCleanup* absl_nullable cleanup = CTrapCleanup::New();
  if (cleanup == nullptr) {
    return KErrNoMemory;
  }
  CActiveScheduler scheduler;
  CActiveScheduler* absl_nullable previous = CActiveScheduler::Current();
  CActiveScheduler::Install(&scheduler);
  TRAPD(result, PulseL(duration_ms, stage));
  CActiveScheduler::Install(previous);
  delete cleanup;
  return result;
}

}  // namespace

struct Vibration::Impl {
  symbian::concurrency::WorkerExecutor* absl_nullable worker = nullptr;
  std::shared_ptr<std::atomic<int>> result =
      std::make_shared<std::atomic<int>>(KErrNone);
  std::shared_ptr<std::atomic<int>> stage =
      std::make_shared<std::atomic<int>>(kQueued);
  std::int64_t started_ns = 0;
  absl::Status failure = absl::OkStatus();
};

Vibration::Vibration() : impl_(new (std::nothrow) Impl) {}

Vibration::~Vibration() {
  if (impl_ != nullptr) {
    delete impl_->worker;
    delete impl_;
  }
}

absl::Status Vibration::Start() {
  if (impl_ == nullptr) {
    return absl::ResourceExhaustedError("vibration owner allocation failed");
  }
  if (impl_->worker != nullptr || !impl_->failure.ok()) {
    return impl_->failure;
  }
  impl_->worker = new (std::nothrow) symbian::concurrency::WorkerExecutor(1);
  if (impl_->worker == nullptr) {
    impl_->failure =
        absl::ResourceExhaustedError("vibration worker allocation failed");
  }
  return impl_->failure;
}

absl::Status Vibration::Pulse(int duration_ms) {
  if (duration_ms <= 0 || duration_ms > 5000) {
    return absl::InvalidArgumentError("Vibration duration must be 1..5000 ms");
  }
  absl::Status ready = Start();
  if (!ready.ok()) {
    return ready;
  }
  ready = status();
  if (!ready.ok()) {
    return ready;
  }
  if (impl_->result->load() == kPending) {
    return absl::ResourceExhaustedError("Vibration request already pending");
  }
  impl_->result->store(kPending);
  impl_->stage->store(kQueued);
  const std::shared_ptr<std::atomic<int>> result = impl_->result;
  const std::shared_ptr<std::atomic<int>> stage = impl_->stage;
  const absl::Status posted = impl_->worker->Post([result, stage, duration_ms] {
    result->store(RunPulse(duration_ms, stage.get()));
  });
  if (!posted.ok()) {
    impl_->result->store(KErrGeneral);
    impl_->failure = posted;
    return posted;
  }
  impl_->started_ns = symbian::api::time::MonotonicClock::NowNanoseconds();
  return absl::OkStatus();
}

absl::Status Vibration::status() {
  if (impl_ == nullptr) {
    return absl::ResourceExhaustedError("vibration owner allocation failed");
  }
  if (!impl_->failure.ok()) {
    return impl_->failure;
  }
  if (impl_->worker == nullptr) {
    return absl::FailedPreconditionError("vibration worker not started");
  }
  const int result = impl_->result->load();
  if (result != KErrNone && result != kPending) {
    impl_->failure = symbian::StatusFromNativeError(result, "HWRM vibration");
    impl_->worker->Close();
  } else if (result == kPending &&
             symbian::api::time::MonotonicClock::NowNanoseconds() -
                     impl_->started_ns >
                 kServiceDeadlineNs) {
    impl_->failure =
        absl::DeadlineExceededError(TimeoutMessage(impl_->stage->load()));
    impl_->worker->Close();
  }
  return impl_->failure;
}

}  // namespace symbian::api::media
