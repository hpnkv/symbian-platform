#include <chrono>
#include <cstdint>
#include <limits>
#include <memory>
#include <thread>

#include "abi.h"
#include "symbian/concurrency/bounded_channel.h"
#include "symbian/concurrency/event_mailbox.h"
#include "symbian/concurrency/property_watch.h"
#include "symbian/concurrency/task_group.h"
#include "symbian/concurrency/timer_pump.h"

namespace {
std::int64_t fake_wall_seconds = 1000000;
int fake_wall_reads = 0;

absl::Time FakeWallNow() {
  ++fake_wall_reads;
  return absl::FromUnixSeconds(fake_wall_seconds);
}
}  // namespace

extern "C" int SymbianRuntimeTimerFutureProbe() {
  using namespace std::chrono_literals;
  using symbian::concurrency::Task;
  using symbian::concurrency::TimerPump;
  using symbian::concurrency::Unit;

  const int cells_before = SymbianRuntimeAllocationCells();
  {
    thread::Mutex mu;
    thread::CondVar cv;
    mu.Lock();
    const bool past = cv.WaitWithDeadline(&mu, absl::Now() - absl::Seconds(1));
    const bool timed = cv.WaitWithTimeout(&mu, absl::Milliseconds(2));
    mu.Unlock();
    if (!past) {
      return -310;
    }
    if (!timed) {
      return -319;
    }
  }
  {
    symbian::concurrency::BoundedChannel<std::unique_ptr<int>> channel(2);
    std::unique_ptr<int> first = std::make_unique<int>(5);
    std::unique_ptr<int> second = std::make_unique<int>(7);
    std::unique_ptr<int> rejected = std::make_unique<int>(9);
    if (!channel.TryWrite(std::move(first)).ok() || first ||
        !channel.TryWrite(std::move(second)).ok() || second ||
        channel.TryWrite(std::move(rejected)).code() !=
            absl::StatusCode::kResourceExhausted ||
        !rejected || *rejected != 9 || channel.Size() != 2) {
      return -311;
    }
    std::unique_ptr<int> received;
    auto read = channel.TryRead(&received);
    if (!read.ok() || !read.value() || !received || *received != 5) {
      return -312;
    }
    read = channel.TryRead(&received);
    if (!read.ok() || !read.value() || !received || *received != 7 ||
        channel.TryRead(&received).status().code() !=
            absl::StatusCode::kUnavailable) {
      return -313;
    }
    channel.Close();
    read = channel.TryRead(&received);
    if (!read.ok() || read.value() ||
        channel.TryWrite(std::move(rejected)).code() !=
            absl::StatusCode::kFailedPrecondition ||
        !rejected || *rejected != 9) {
      return -314;
    }
  }
  {
    symbian::concurrency::BoundedChannel<int> channel(1);
    absl::Status producer_status;
    std::thread producer([&] {
      producer_status = channel.writer()->Write(11);
      if (producer_status.ok()) {
        producer_status = channel.writer()->Write(13);
      }
      channel.writer()->Close();
    });
    int value = 0;
    const bool first = channel.reader()->Read(&value) && value == 11;
    const bool second = channel.reader()->Read(&value) && value == 13;
    const bool ended = !channel.reader()->Read(&value);
    producer.join();
    if (!producer_status.ok() || !first || !second || !ended) {
      return -315;
    }
  }
  {
    symbian::concurrency::BoundedChannel<std::unique_ptr<int>> channel(1);
    if (!channel.writer()->Write(std::make_unique<int>(1)).ok()) {
      return -316;
    }
    std::unique_ptr<int> blocked = std::make_unique<int>(2);
    absl::Status blocked_status;
    std::thread producer(
        [&] { blocked_status = channel.writer()->Write(std::move(blocked)); });
    channel.Close();
    producer.join();
    if (blocked_status.code() != absl::StatusCode::kFailedPrecondition ||
        !blocked || *blocked != 2) {
      return -317;
    }
    std::unique_ptr<int> drained;
    if (!channel.reader()->Read(&drained) || !drained || *drained != 1 ||
        channel.reader()->Read(&drained)) {
      return -318;
    }
  }
  {
    TimerPump pump;
    if (pump.Open() != 0) {
      return -260;
    }
    Task cancelled = pump.ScheduleAfter(absl::Seconds(2));
    if (!cancelled.valid() || cancelled.IsReady()) {
      return -261;
    }
    const auto start = std::chrono::steady_clock::now();
    std::thread worker([cancelled, start] {
      while (std::chrono::steady_clock::now() - start < 20ms) {}
      cancelled.Cancel();
    });
    while (!cancelled.IsReady()) {
      pump.DispatchReady();
      if (!cancelled.IsReady()) {
        pump.Park();
      }
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    worker.join();
    if (elapsed >= 500ms || cancelled.ResultIfReady()->ok() ||
        cancelled.ResultIfReady()->status().code() !=
            absl::StatusCode::kCancelled) {
      return -262;
    }

    Task child;
    int callbacks = 0;
    Task immediate = pump.ScheduleAt(absl::Now());
    immediate.OnReady([&](const absl::StatusOr<Unit>& result) {
      if (result.ok()) {
        ++callbacks;
        child = pump.ScheduleAfter(absl::ZeroDuration());
      }
    });
    Task later = pump.ScheduleAfter(absl::Milliseconds(30));
    Task deadline = pump.ScheduleAt(absl::Now() + absl::Milliseconds(15));
    auto continued = symbian::concurrency::Then(
        later, [](const absl::StatusOr<Unit>& result) -> absl::StatusOr<int> {
          return result.ok() ? absl::StatusOr<int>(42)
                             : absl::StatusOr<int>(result.status());
        });
    while (!immediate.IsReady() || !later.IsReady() || !deadline.IsReady() ||
           !child.valid() || !child.IsReady()) {
      pump.DispatchReady();
      if (!immediate.IsReady() || !later.IsReady() || !deadline.IsReady() ||
          !child.valid() || !child.IsReady()) {
        pump.Park();
      }
    }
    if (!immediate.ResultIfReady()->ok() || !child.ResultIfReady()->ok() ||
        !later.ResultIfReady()->ok() || !deadline.ResultIfReady()->ok() ||
        callbacks != 1) {
      return -263;
    }
    if (!continued.IsReady() || !continued.ResultIfReady()->ok() ||
        continued.ResultIfReady()->value() != 42) {
      return -269;
    }
    immediate.OnReady([&](const absl::StatusOr<Unit>&) { ++callbacks; });
    if (callbacks != 2) {
      return -264;
    }
    Task pending = pump.ScheduleAfter(absl::Seconds(2));
    pump.Close();
    if (!pending.IsReady() || pending.ResultIfReady()->ok() ||
        pending.ResultIfReady()->status().code() !=
            absl::StatusCode::kCancelled ||
        pending.Cancel() != true) {
      return -266;
    }
    if (pump.ScheduleAfter(absl::Microseconds(1))
            .ResultIfReady()
            ->status()
            .code() != absl::StatusCode::kFailedPrecondition) {
      return -267;
    }
  }
  {
    if (symbian::concurrency::internal::TimerSliceMicroseconds(
            absl::Hours(24)) != std::numeric_limits<std::int32_t>::max() ||
        symbian::concurrency::internal::TimerSliceMicroseconds(
            absl::Nanoseconds(1)) != 1) {
      return -282;
    }
    TimerPump pump(4, &FakeWallNow);
    if (pump.Open() != 0) {
      return -283;
    }
    Task adjusted = pump.ScheduleAt(FakeWallNow() + absl::Milliseconds(15));
    if (fake_wall_reads != 2 || adjusted.IsReady()) {
      return -284;
    }
    fake_wall_seconds += 3600;
    Task long_wait = pump.ScheduleAfter(absl::Hours(24));
    Task forever = pump.ScheduleAt(absl::InfiniteFuture());
    if (long_wait.IsReady() || forever.IsReady() || fake_wall_reads != 3 ||
        !pump.ScheduleAfter(-absl::Microseconds(1)).IsReady()) {
      return -285;
    }
    long_wait.Cancel();
    forever.Cancel();
    while (!adjusted.IsReady() || !long_wait.IsReady() || !forever.IsReady()) {
      pump.DispatchReady();
      if (!adjusted.IsReady() || !long_wait.IsReady() || !forever.IsReady()) {
        pump.Park();
      }
    }
    if (!adjusted.ResultIfReady()->ok() ||
        long_wait.ResultIfReady()->status().code() !=
            absl::StatusCode::kCancelled ||
        forever.ResultIfReady()->status().code() !=
            absl::StatusCode::kCancelled ||
        fake_wall_reads != 3) {
      return -286;
    }
    pump.Close();
  }
  {
    TimerPump bounded(2);
    if (bounded.Open() != 0) {
      return -270;
    }
    Task first = bounded.ScheduleAfter(absl::Milliseconds(30));
    Task second = bounded.ScheduleAfter(absl::Milliseconds(30));
    if (!first.valid() || first.IsReady() || !second.valid() ||
        second.IsReady()) {
      return -271;
    }
    Task rejected = bounded.ScheduleAfter(absl::Milliseconds(30));
    if (!rejected.IsReady() || rejected.ResultIfReady()->ok() ||
        rejected.ResultIfReady()->status().code() !=
            absl::StatusCode::kResourceExhausted) {
      return -272;
    }
    while (!first.IsReady()) {
      bounded.DispatchReady();
      if (!first.IsReady()) {
        bounded.Park();
      }
    }
    Task replacement = bounded.ScheduleAfter(absl::Milliseconds(30));
    if (!replacement.valid() || replacement.IsReady()) {
      return -273;
    }
    bounded.Close();
    if (!replacement.IsReady() || replacement.ResultIfReady()->ok() ||
        replacement.ResultIfReady()->status().code() !=
            absl::StatusCode::kCancelled) {
      return -274;
    }
  }
  {
    TimerPump event_thread;
    if (event_thread.Open() != 0) {
      return -291;
    }
    symbian::concurrency::EventMailbox mailbox(event_thread.WakeCallback(), 1);
    const auto event_id = std::this_thread::get_id();
    int calls = 0;
    bool wrong_thread = false;
    absl::Status submitted;
    std::thread worker([&] {
      submitted = mailbox.Enqueue([&] {
        if (std::this_thread::get_id() != event_id) {
          wrong_thread = true;
        }
        ++calls;
        if (!mailbox.Enqueue([&] { ++calls; }).ok()) {
          wrong_thread = true;
        }
      });
    });
    worker.join();
    if (!submitted.ok() ||
        mailbox.Enqueue([] {}).code() != absl::StatusCode::kResourceExhausted) {
      return -292;
    }
    event_thread.Park();
    if (mailbox.DispatchReady(1) != 1 || calls != 1 || mailbox.Pending() != 1 ||
        wrong_thread) {
      return -293;
    }
    if (mailbox.DispatchReady(1) != 1 || calls != 2 || mailbox.Pending() != 0) {
      return -294;
    }
    mailbox.Close();
    if (mailbox.Enqueue([] {}).code() !=
        absl::StatusCode::kFailedPrecondition) {
      return -295;
    }
    event_thread.Close();
  }
  {
    TimerPump event_thread;
    if (event_thread.Open() != 0) {
      return -287;
    }
    symbian::concurrency::PropertyWatch property(event_thread.WakeCallback());
    const absl::Status opened = property.Open(0xe0000813, 0x5151);
    if (!opened.ok()) {
      return symbian::NativeErrorFromStatus(opened);
    }
    auto first = property.Next();
    auto duplicate = property.Next();
    if (!duplicate.IsReady() || duplicate.ResultIfReady()->status().code() !=
                                    absl::StatusCode::kFailedPrecondition) {
      return -288;
    }
    auto set = property.Set(11);
    if (!set.ok()) {
      return symbian::NativeErrorFromStatus(set);
    }
    int callback_count = 0;
    int callback_error = 0;
    symbian::concurrency::Future<int> reentrant;
    first.OnReady([&](const absl::StatusOr<int>& result) {
      if (result.ok() && result.value() == 11) {
        ++callback_count;
        reentrant = property.Next();
        if (!property.Set(22).ok()) {
          callback_error = 1;
        }
      }
    });
    while (!first.IsReady() || !reentrant.valid() || !reentrant.IsReady()) {
      property.DispatchReady();
      event_thread.DispatchReady();
      if (!first.IsReady() || !reentrant.valid() || !reentrant.IsReady()) {
        event_thread.Park();
      }
    }
    if (callback_error != 0 || callback_count != 1 ||
        !reentrant.ResultIfReady()->ok() ||
        reentrant.ResultIfReady()->value() != 22) {
      return -289;
    }
    auto cancelled = property.Next();
    std::thread cancel_worker([cancelled] { cancelled.Cancel(); });
    cancel_worker.join();
    while (!cancelled.IsReady()) {
      property.DispatchReady();
      event_thread.DispatchReady();
      if (!cancelled.IsReady()) {
        event_thread.Park();
      }
    }
    if (cancelled.ResultIfReady()->status().code() !=
        absl::StatusCode::kCancelled) {
      return -290;
    }
    auto closed_pending = property.Next();
    property.Close();
    if (!closed_pending.IsReady() ||
        closed_pending.ResultIfReady()->status().code() !=
            absl::StatusCode::kCancelled) {
      return -296;
    }
    event_thread.Close();
  }
  {
    TimerPump pump(3);
    if (pump.Open() != 0) {
      return -275;
    }
    symbian::concurrency::TaskGroup group;
    if (!group.Add(pump.ScheduleAfter(absl::Milliseconds(10))) ||
        !group.Add(pump.ScheduleAfter(absl::Milliseconds(20)))) {
      return -276;
    }
    Task joined = group.Finish();
    if (joined.IsReady()) {
      return -277;
    }
    while (!joined.IsReady()) {
      pump.DispatchReady();
      if (!joined.IsReady()) {
        pump.Park();
      }
    }
    if (!joined.ResultIfReady()->ok()) {
      return -278;
    }

    symbian::concurrency::TaskGroup cancelled_group;
    if (!cancelled_group.Add(pump.ScheduleAfter(absl::Seconds(2))) ||
        !cancelled_group.Add(pump.ScheduleAfter(absl::Seconds(2))) ||
        !cancelled_group.Add(pump.ScheduleAfter(absl::Seconds(2)))) {
      return -279;
    }
    Task cancelled_join = cancelled_group.Finish();
    if (!cancelled_join.Cancel() || cancelled_join.IsReady()) {
      return -280;
    }
    while (!cancelled_join.IsReady()) {
      pump.DispatchReady();
      if (!cancelled_join.IsReady()) {
        pump.Park();
      }
    }
    if (cancelled_join.ResultIfReady()->ok() ||
        cancelled_join.ResultIfReady()->status().code() !=
            absl::StatusCode::kCancelled) {
      return -281;
    }
    pump.Close();
  }
  if (SymbianRuntimeAllocationCells() != cells_before) {
    return -268;
  }
#ifdef SYMBIAN_RUNTIME_CHANGED_TIMER_FUTURE
  return -265;
#else
  return 0;
#endif
}
