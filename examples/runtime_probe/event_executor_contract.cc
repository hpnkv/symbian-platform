#include <atomic>
#include <chrono>
#include <thread>

#include "abi.h"
#include "symbian/concurrency/event_executor.h"
#include "symbian/concurrency/native_task_owner.h"
#include "symbian/concurrency/task_group.h"

extern "C" int SymbianRuntimeEventExecutorProbe() {
  using symbian::concurrency::EventExecutor;
  using symbian::concurrency::TaskGroup;
  using symbian::concurrency::Unit;
  const int before = SymbianRuntimeAllocationCells();
  int error = 0;
  {
    EventExecutor executor;
    if (!executor.Open().ok() ||
        !executor.OpenProperty(0xe0000813, 0x5152).ok()) {
      return -330;
    }
    auto property = executor.NextProperty();
    auto timer = executor.ScheduleAfter(absl::Milliseconds(10));
    TaskGroup group;
    if (!group.Add(timer) ||
        !group.Add(symbian::concurrency::Then(
            property,
            [](const absl::StatusOr<int>& result) -> absl::StatusOr<Unit> {
              if (!result.ok()) {
                return result.status();
              }
              if (*result != 17) {
                return absl::InternalError("Unexpected property value");
              }
              return Unit{};
            }))) {
      return -331;
    }
    auto joined = group.Finish();
    auto repeated = group.Finish();
    if (joined.IsReady() || repeated.IsReady() ||
        joined.Await().status().code() !=
            absl::StatusCode::kFailedPrecondition) {
      return -332;
    }
    int callback_count = 0;
    joined.OnReady([&](const auto& result) {
      ++callback_count;
      if (!result.ok()) {
        error = -333;
      }
    });
    thread::Fiber waiter(executor.fibers(), [&] {
      const auto result = joined.Await(absl::Now() + absl::Seconds(2));
      if (!result.ok()) {
        error = -334;
      }
    });
    if (!executor.SetProperty(17).ok()) {
      return -335;
    }
    for (int turn = 0; turn < 128 && !waiter.Finished(); ++turn) {
      if (!executor.DispatchReady().ok()) {
        return -336;
      }
      if (!waiter.Finished()) {
        executor.Park();
      }
    }
    if (!waiter.Finished() || !joined.IsReady() || !repeated.IsReady() ||
        callback_count != 1 || error != 0) {
      return -337;
    }

    // A fiber's monotonic sleep must arm the same native request owner.
    bool slept = false;
    thread::Fiber sleeper(executor.fibers(), [&] {
      thread::Fiber::SleepFor(absl::Milliseconds(5));
      slept = true;
    });
    for (int turn = 0; turn < 128 && !sleeper.Finished(); ++turn) {
      if (!executor.DispatchReady().ok()) {
        return -338;
      }
      if (!sleeper.Finished()) {
        executor.Park();
      }
    }
    if (!sleeper.Finished() || !slept) {
      return -339;
    }
    auto pending = executor.NextProperty();
    pending.Cancel();
    for (int turn = 0; turn < 128 && !pending.IsReady(); ++turn) {
      if (!executor.DispatchReady().ok()) {
        return -340;
      }
      if (!pending.IsReady()) {
        executor.Park();
      }
    }
    if (!pending.IsReady() || pending.ResultIfReady()->status().code() !=
                                  absl::StatusCode::kCancelled) {
      return -341;
    }
    symbian::concurrency::Future<int> next;
    int reentrant = 0;
    auto first = executor.NextProperty();
    first.OnReady([&](const absl::StatusOr<int>& result) {
      if (!result.ok() || *result != 21) {
        error = -345;
        return;
      }
      ++reentrant;
      next = executor.NextProperty();
      if (!executor.SetProperty(22).ok()) {
        error = -346;
      }
    });
    if (!executor.SetProperty(21).ok()) {
      return -347;
    }
    for (int turn = 0; turn < 128 && (!next.valid() || !next.IsReady());
         ++turn) {
      if (!executor.DispatchReady().ok()) {
        return -348;
      }
      if (!next.valid() || !next.IsReady()) {
        executor.Park();
      }
    }
    if (!next.valid() || !next.IsReady() || !next.ResultIfReady()->ok() ||
        next.ResultIfReady()->value() != 22 ||
        !next.Await(absl::InfinitePast()).ok() || reentrant != 1 || error) {
      return -349;
    }

    auto deadline_pending = executor.NextProperty();
    bool timed_out = false;
    thread::Fiber deadline_waiter(executor.fibers(), [&] {
      timed_out = deadline_pending.Await(absl::Now() + absl::Milliseconds(5))
                      .status()
                      .code() == absl::StatusCode::kDeadlineExceeded;
    });
    for (int turn = 0; turn < 128 && !deadline_waiter.Finished(); ++turn) {
      if (!executor.DispatchReady().ok()) {
        return -350;
      }
      if (!deadline_waiter.Finished()) {
        executor.Park();
      }
    }
    if (!deadline_waiter.Finished() || !timed_out ||
        deadline_pending.IsReady() || !executor.SetProperty(23).ok()) {
      return -351;
    }
    for (int turn = 0; turn < 128 && !deadline_pending.IsReady(); ++turn) {
      if (!executor.DispatchReady().ok()) {
        return -352;
      }
      if (!deadline_pending.IsReady()) {
        executor.Park();
      }
    }
    if (!deadline_pending.IsReady() ||
        !deadline_pending.ResultIfReady()->ok() ||
        deadline_pending.ResultIfReady()->value() != 23) {
      return -353;
    }
    symbian::concurrency::Future<int> abandoned;
    {
      symbian::concurrency::Promise<int> producer;
      abandoned = producer.future();
    }
    if (!abandoned.IsReady() || abandoned.ResultIfReady()->status().code() !=
                                    absl::StatusCode::kCancelled) {
      return -354;
    }
    {
      symbian::concurrency::NativeTaskOwner owner(executor);
      auto done =
          owner.Start(absl::Milliseconds(5), absl::Now() + absl::Seconds(2));
      auto value = owner.property_result();
      if (!executor.SetProperty(31).ok()) {
        return -356;
      }
      for (int turn = 0; turn < 128 && !done.IsReady(); ++turn) {
        if (!executor.DispatchReady().ok()) {
          return -357;
        }
        if (!done.IsReady()) {
          executor.Park();
        }
      }
      if (!done.IsReady() || !done.ResultIfReady()->ok() ||
          !value.ResultIfReady()->ok() ||
          value.ResultIfReady()->value() != 31 || !owner.Finish().IsReady()) {
        return -358;
      }
      owner.Close();
      owner.Close();
    }
    {
      symbian::concurrency::NativeTaskOwner owner(executor);
      auto done = owner.Start(absl::InfiniteDuration());
      owner.Close();
      owner.Close();
      for (int turn = 0; turn < 128 && !done.IsReady(); ++turn) {
        if (!executor.DispatchReady().ok()) {
          return -359;
        }
        if (!done.IsReady()) {
          executor.Park();
        }
      }
      if (!done.IsReady() || done.ResultIfReady()->status().code() !=
                                 absl::StatusCode::kCancelled) {
        return -360;
      }
    }
    {
      symbian::concurrency::NativeTaskOwner owner(executor);
      auto done = owner.Start(absl::InfiniteDuration(),
                              absl::Now() + absl::Milliseconds(5));
      for (int turn = 0; turn < 128 && !done.IsReady(); ++turn) {
        if (!executor.DispatchReady().ok()) {
          return -361;
        }
        if (!done.IsReady()) {
          executor.Park();
        }
      }
      if (!done.IsReady() || done.ResultIfReady()->status().code() !=
                                 absl::StatusCode::kDeadlineExceeded) {
        return -362;
      }
    }
    {
      symbian::concurrency::NativeTaskOwner owner(executor);
      auto done = owner.Start(-absl::Milliseconds(1));
      for (int turn = 0; turn < 128 && !done.IsReady(); ++turn) {
        if (!executor.DispatchReady().ok()) {
          return -363;
        }
        if (!done.IsReady()) {
          executor.Park();
        }
      }
      if (!done.IsReady() || done.ResultIfReady()->status().code() !=
                                 absl::StatusCode::kInvalidArgument) {
        return -364;
      }
    }
    symbian::concurrency::Task after_owner_destruction;
    {
      symbian::concurrency::NativeTaskOwner owner(executor);
      after_owner_destruction = owner.Start(absl::InfiniteDuration());
    }
    if (after_owner_destruction.IsReady()) {
      return -365;
    }
    for (int turn = 0; turn < 128 && !after_owner_destruction.IsReady();
         ++turn) {
      if (!executor.DispatchReady().ok()) {
        return -366;
      }
      if (!after_owner_destruction.IsReady()) {
        executor.Park();
      }
    }
    if (!after_owner_destruction.IsReady() ||
        after_owner_destruction.ResultIfReady()->status().code() !=
            absl::StatusCode::kCancelled) {
      return -367;
    }
    int dispatches = 0;
    if (!executor.DispatchToEvent([&] { ++dispatches; }).ok() ||
        !executor.DispatchReady().ok() || dispatches != 1) {
      return -342;
    }
    std::atomic<int> submitted{0};
    std::thread worker([&] {
      const absl::Status status =
          executor.DispatchToEvent([&] { ++dispatches; });
      submitted.store(status.ok() ? 1 : -1, std::memory_order_release);
    });
    executor.Park();
    worker.join();
    if (submitted.load(std::memory_order_acquire) != 1 ||
        !executor.DispatchReady().ok() || dispatches != 2) {
      return -355;
    }
    executor.Close();
  }
  if (SymbianRuntimeAllocationCells() != before) {
    return -343;
  }
#ifdef SYMBIAN_RUNTIME_CHANGED_EVENT_EXECUTOR
  return -344;
#else
  return 0;
#endif
}
