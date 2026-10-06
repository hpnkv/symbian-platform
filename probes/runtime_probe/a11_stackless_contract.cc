#include <atomic>
#include <memory>
#include <thread>

#include <absl/base/nullability.h>

#include "abi.h"
#include "symbian/concurrency/future.h"
#include "symbian/concurrency/inline_pump.h"
#include "symbian/concurrency/parallel.h"
#include "symbian/concurrency/task_group.h"

extern "C" int SymbianRuntimeA11StacklessProbe() {
  using symbian::concurrency::Promise;
  const int before = SymbianRuntimeAllocationCells();

  {
    Promise<int> producer;
    auto source = producer.future();
    std::atomic<int> callback_count{0};
    auto continued = symbian::concurrency::Then(
        source, [](const absl::StatusOr<int>& input) {
          return absl::StatusOr<int>(input.ok() ? input.value() + 14 : -1);
        });
    source.OnReady([&](const absl::StatusOr<int>& result) {
      if (result.ok() && result.value() == 2026) {
        ++callback_count;
      }
    });
    std::thread worker([producer = std::move(producer)]() mutable {
      producer.SetValue(2026);
    });
    worker.join();
    if (callback_count.load() != 1 || !continued.IsReady() ||
        continued.ResultIfReady()->value() != 2040) {
      return -160;
    }
    if (!continued.Await().ok() || continued.Await().value() != 2040) {
      return -178;
    }
    // A11's ready path is allowed to call back on this very stack.
    source.OnReady([&](const absl::StatusOr<int>&) { ++callback_count; });
    if (callback_count.load() != 2) {
      return -161;
    }
  }

  {
    Promise<int> producer;
    auto source = producer.future();
    std::atomic<int> requested{0};
    producer.SetCancellationCallback([&] { requested.store(1); });
    if (source.Await(absl::Now() + absl::Milliseconds(1)).status().code() !=
        absl::StatusCode::kFailedPrecondition) {
      return -179;
    }
    if (!source.Cancel() || requested.load() != 1 || source.IsReady()) {
      return -162;
    }
    if (!producer.SetError({absl::StatusCode::kCancelled, "requested"}) ||
        producer.SetValue(1) ||
        source.ResultIfReady()->status().code() !=
            absl::StatusCode::kCancelled ||
        !source.Cancel()) {
      return -163;
    }
  }

  {
    symbian::concurrency::Future<int> abandoned;
    {
      Promise<int> producer;
      abandoned = producer.future();
    }
    if (!abandoned.IsReady() || abandoned.ResultIfReady()->ok() ||
        abandoned.ResultIfReady()->status().code() !=
            absl::StatusCode::kCancelled) {
      return -164;
    }
  }

  {
    auto ready = symbian::concurrency::ReadyTask();
    if (!ready.IsReady() || !ready.ResultIfReady()->ok()) {
      return -165;
    }
  }

  {
    Promise<int> first;
    Promise<int> second;
    auto joined =
        symbian::concurrency::JoinAll<int>({first.future(), second.future()});
    std::atomic<int> callbacks{0};
    joined.OnReady(
        [&](const absl::StatusOr<std::vector<absl::StatusOr<int>>>& result) {
          if (result.ok()) {
            ++callbacks;
          }
        });
    second.SetError({absl::StatusCode::kCancelled, "second cancelled"});
    if (joined.IsReady() || callbacks.load() != 0) {
      return -168;
    }
    first.SetValue(7);
    auto results = joined.ResultIfReady();
    if (!results || !results->ok() || results->value().size() != 2 ||
        results->value()[0].value() != 7 ||
        results->value()[1].status().code() != absl::StatusCode::kCancelled ||
        callbacks.load() != 1) {
      return -169;
    }
  }

  {
    Promise<int> first;
    Promise<int> second;
    int cancelled = 0;
    first.SetCancellationCallback([&] { ++cancelled; });
    second.SetCancellationCallback([&] { ++cancelled; });
    auto joined =
        symbian::concurrency::JoinAll<int>({first.future(), second.future()});
    if (!joined.Cancel() || cancelled != 2 || joined.IsReady()) {
      return -170;
    }
    first.SetValue(1);
    second.SetValue(2);
    if (!joined.IsReady() || !joined.Cancel()) {
      return -171;
    }
    auto empty = symbian::concurrency::JoinAll<int>({});
    if (!empty.IsReady() || !empty.ResultIfReady()->value().empty()) {
      return -172;
    }
  }

  {
    thread::Mutex mu;
    symbian::concurrency::InlinePumpState pump;
    int calls = 0;
    int observed_depth = 0;
    std::function<void()> once;
    once = [&] {
      ++calls;
      {
        thread::MutexLock lock(&mu);
        if (!symbian::concurrency::PumpIsDriving(pump)) {
          observed_depth = -1;
        }
        if (static_cast<int>(pump.depth) > observed_depth) {
          observed_depth = static_cast<int>(pump.depth);
        }
      }
      if (calls <= 4) {
        symbian::concurrency::DriveInline(&mu, &pump, "probe", once, 2);
      }
    };
    symbian::concurrency::DriveInline(&mu, &pump, "probe", once, 2);
    if (calls != 5 || observed_depth != 2 || pump.depth != 0 || pump.again ||
        symbian::concurrency::PumpIsDriving(pump)) {
      return -173;
    }
  }

  {
    Promise<symbian::concurrency::Unit> first;
    Promise<symbian::concurrency::Unit> second;
    symbian::concurrency::TaskGroup group;
    if (!group.Add(first.future()) || !group.Add(second.future())) {
      return -174;
    }
    auto done = group.Finish();
    if (group.Add(symbian::concurrency::ReadyTask()) ||
        group.Finish().IsReady()) {
      return -175;
    }
    second.SetError({absl::StatusCode::kCancelled, "second cancelled"});
    if (done.IsReady()) {
      return -176;
    }
    first.SetValue({});
    if (!done.IsReady() || done.ResultIfReady()->ok() ||
        done.ResultIfReady()->status().code() != absl::StatusCode::kCancelled ||
        !group.Finish().IsReady()) {
      return -177;
    }
  }

  {
    Promise<symbian::concurrency::Unit> producer;
    int cancelled = 0;
    producer.SetCancellationCallback([&] { ++cancelled; });
    symbian::concurrency::TaskGroup group;
    group.Add(producer.future());
    auto done = group.Finish();
    group.Cancel();
    if (cancelled != 1 || done.IsReady()) {
      return -178;
    }
    producer.SetValue({});
    if (!done.IsReady() || !done.ResultIfReady()->ok()) {
      return -179;
    }
  }

  {
    Promise<symbian::concurrency::Unit> producer;
    int cancelled = 0;
    producer.SetCancellationCallback([&] { ++cancelled; });
    {
      symbian::concurrency::TaskGroup group;
      group.Add(producer.future());
    }
    if (cancelled != 1) {
      return -180;
    }
    producer.SetValue({});
  }

#ifdef SYMBIAN_RUNTIME_CHANGED_A11_STACKLESS
  return -166;
#else
  return SymbianRuntimeAllocationCells() == before ? 0 : -167;
#endif
}
