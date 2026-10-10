#include <atomic>
#include <chrono>
#include <memory>
#include <span>
#include <string>
#include <thread>

#include <absl/base/nullability.h>

#include "abi.h"
#include "symbian/concurrency/future.h"
#include "thread/boost_primitives.h"
#include "thread/channel.h"
#include "thread/fiber.h"

namespace {
class LifoPolicy final {
 public:
  LifoPolicy()
      : callbacks{.context = this,
                  .pick_next =
                      [](void* absl_nonnull,
                         std::span<thread::Fiber* absl_nonnull const> ready) {
                        return ready.size() - 1;
                      },
                  .notify_ready =
                      [](void* absl_nonnull context) noexcept {
                        ++static_cast<LifoPolicy*>(context)->notifications;
                      }} {}

  thread::SchedulerPolicy callbacks;
  std::atomic<int> notifications{0};
};
}  // namespace

extern "C" int SymbianRuntimeFiberLocksProbe() {
  const int before = SymbianRuntimeAllocationCells();
  int result = 0;
  {
    thread::Scheduler scheduler;
    thread::Mutex mutex;
    thread::CondVar condition;
    int phase = 0;
    bool ready = false;
    symbian::concurrency::Promise<int> promise;
    auto future = promise.future();
    if (future.Await().status().code() !=
        absl::StatusCode::kFailedPrecondition) {
      return -310;
    }
    auto owned = std::make_unique<std::string>(96, 'a');
    thread::Fiber first(&scheduler, [&, owned = std::move(owned)] {
      thread::MutexLock lock(&mutex);
      phase = 1;
      thread::Fiber::Yield();
      if (phase != 1 || (*owned)[95] != 'a') {
        result = -311;
      }
      phase = 2;
    });
    thread::Fiber second(&scheduler, [&] {
      thread::MutexLock lock(&mutex);
      if (phase != 2) {
        result = -312;
      }
      ready = true;
      condition.Signal();
      promise.SetValue(42);
    });
    thread::Fiber third(&scheduler, [&] {
      thread::MutexLock lock(&mutex);
      while (!ready) {
        condition.Wait(&mutex);
      }
      if (phase != 2) {
        result = -313;
      }
    });
    thread::Fiber fourth(&scheduler, [&] {
      auto value = future.Await();
      if (!value.ok() || *value != 42) {
        result = -314;
      }
    });
    if (!scheduler.RunReady(32).ok() || !first.Finished() ||
        !second.Finished() || !third.Finished() || !fourth.Finished() ||
        result != 0) {
      return result == 0 ? -315 : result;
    }

    thread::Fiber timeout(&scheduler, [&] {
      thread::MutexLock lock(&mutex);
      if (!condition.WaitWithTimeout(&mutex, absl::Milliseconds(2))) {
        result = -316;
      }
    });
    if (!scheduler.RunReady(1).ok() || timeout.Finished() ||
        scheduler.HasReady() ||
        scheduler.NextDeadline() ==
            std::chrono::steady_clock::time_point::max()) {
      return -317;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(4));
    if (!scheduler.RunReady(2).ok() || !timeout.Finished() || result != 0) {
      return result == 0 ? -318 : result;
    }

    thread::Channel<int> channel(1);
    int received = 0;
    thread::Fiber reader(&scheduler, [&] {
      if (!channel.reader()->Read(&received)) {
        result = -324;
      }
    });
    thread::Fiber writer(&scheduler, [&] { channel.writer()->Write(37); });
    if (!scheduler.RunReady(4).ok() || !reader.Finished() ||
        !writer.Finished() || received != 37 || result != 0) {
      return result == 0 ? -326 : result;
    }
  }
  {
    LifoPolicy policy;
    thread::Scheduler scheduler(&policy.callbacks);
    int order = 0;
    thread::Fiber first(&scheduler, [&] { order = order * 10 + 1; });
    thread::Fiber second(&scheduler, [&] { order = order * 10 + 2; });
    if (!scheduler.RunReady(2).ok() || order != 21 ||
        policy.notifications.load() < 2) {
      return -321;
    }

    thread::Mutex mutex;
    thread::CondVar condition;
    bool notified = false;
    thread::Fiber waiter(&scheduler, [&] {
      thread::MutexLock lock(&mutex);
      while (!notified) {
        condition.Wait(&mutex);
      }
      order = order * 10 + 3;
    });
    if (!scheduler.RunReady(1).ok() || waiter.Finished() ||
        scheduler.HasReady()) {
      return -322;
    }
    std::thread worker([&] {
      thread::MutexLock lock(&mutex);
      notified = true;
      condition.Signal();
    });
    worker.join();
    if (!scheduler.RunReady(2).ok() || !waiter.Finished() || order != 213 ||
        policy.notifications.load() < 4) {
      return -323;
    }
  }
  if (SymbianRuntimeAllocationCells() != before) {
    return -319;
  }
#ifdef SYMBIAN_RUNTIME_CHANGED_FIBER_LOCKS
  return -320;
#else
  return 0;
#endif
}
