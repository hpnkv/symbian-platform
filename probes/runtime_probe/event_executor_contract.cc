#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <type_traits>
#include <utility>

#include "abi.h"
#include "symbian/concurrency/event_executor.h"
#include "symbian/concurrency/native_task_owner.h"
#include "symbian/concurrency/task_group.h"
#include "symbian/concurrency/worker_executor.h"
#include "thread/channel.h"
#include "thread/select.h"
#include "thread/selectables.h"

static_assert(std::is_same_v<decltype(std::declval<thread::Reader<int>&>().Read(
                                 std::declval<int*>())),
                             bool>);
static_assert(std::is_same_v<
              decltype(std::declval<thread::Writer<int>&>().Write(1)), void>);
static_assert(std::is_same_v<decltype(std::declval<thread::Writer<int>&>()
                                          .WriteUnlessCancelled(1)),
                             bool>);
static_assert(!std::is_copy_constructible_v<thread::Reader<int>>);
static_assert(!std::is_copy_constructible_v<thread::Writer<int>>);

extern "C" int SymbianRuntimeEventExecutorProbe() {
  using symbian::concurrency::EventExecutor;
  using symbian::concurrency::TaskGroup;
  using symbian::concurrency::Unit;
  const auto event_thread = std::this_thread::get_id();
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
            [event_thread](
                const absl::StatusOr<int>& result) -> absl::StatusOr<Unit> {
              if (std::this_thread::get_id() != event_thread) {
                return absl::InternalError(
                    "Property continuation left event thread");
              }
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
      if (std::this_thread::get_id() != event_thread) {
        error = -379;
      }
      if (!result.ok()) {
        error = -333;
      }
    });
    thread::Fiber waiter(executor.fibers(), [&] {
      if (std::this_thread::get_id() != event_thread) {
        error = -380;
      }
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
    {
      thread::PermanentEvent event;
      if (thread::Select({thread::AlwaysSelectableCase(), event.OnEvent()}) !=
          0) {
        return -368;
      }
      int selected = -2;
      thread::Fiber waiter(executor.fibers(), [&] {
        selected = thread::SelectUntil(absl::Now() + absl::Milliseconds(5),
                                       {event.OnEvent()});
      });
      for (int turn = 0; turn < 128 && !waiter.Finished(); ++turn) {
        if (!executor.DispatchReady().ok()) {
          return -369;
        }
        if (!waiter.Finished()) {
          executor.Park();
        }
      }
      if (!waiter.Finished() || selected != -1) {
        return -370;
      }
      for (int iteration = 0; iteration < 500; ++iteration) {
        if (thread::SelectUntil(absl::InfinitePast(), {event.OnEvent()}) !=
            -1) {
          return -371;
        }
      }
      event.Notify();
      if (thread::SelectUntil(absl::InfinitePast(), {event.OnEvent()}) != 0 ||
          thread::SelectUntil(absl::Now() - absl::Seconds(1),
                              {event.OnEvent()}) != 0) {
        return -372;
      }
    }
    {
      thread::PermanentEvent event;
      int selected = -2;
      thread::Fiber waiter(executor.fibers(), [&] {
        selected = thread::SelectUntil(absl::Now() + absl::Seconds(1),
                                       {event.OnEvent()});
      });
      if (!executor.DispatchReady().ok()) {
        return -373;
      }
      std::thread notifier([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        event.Notify();
      });
      for (int turn = 0; turn < 128 && !waiter.Finished(); ++turn) {
        if (!executor.DispatchReady().ok()) {
          return -374;
        }
        if (!waiter.Finished()) {
          executor.Park();
        }
      }
      notifier.join();
      if (!waiter.Finished() || selected != 0) {
        return -375;
      }
    }
    {
      thread::PermanentEvent first_event;
      thread::PermanentEvent second_event;
      int selected = -2;
      thread::Fiber waiter(executor.fibers(), [&] {
        selected = thread::SelectUntil(
            absl::Now() + absl::Seconds(1),
            {first_event.OnEvent(), second_event.OnEvent()});
      });
      if (!executor.DispatchReady().ok()) {
        return -376;
      }
      first_event.Notify();
      second_event.Notify();
      for (int turn = 0; turn < 128 && !waiter.Finished(); ++turn) {
        if (!executor.DispatchReady().ok()) {
          return -377;
        }
      }
      if (!waiter.Finished() || selected < 0 || selected > 1 ||
          thread::SelectUntil(absl::InfinitePast(),
                              {first_event.OnEvent(), second_event.OnEvent()}) <
              0) {
        return -378;
      }
    }
    {
      thread::Channel<std::unique_ptr<int>> channel(1);
      channel.writer()->Write(std::make_unique<int>(11));
      auto losing = std::make_unique<int>(23);
      if (thread::Select({thread::AlwaysSelectableCase(),
                          channel.writer()->OnWrite(std::move(losing))}) != 0 ||
          !losing || *losing != 23 || channel.length() != 1) {
        return -401;
      }
      std::unique_ptr<int> received;
      bool read_ok = false;
      if (thread::Select({channel.reader()->OnRead(&received, &read_ok)}) !=
              0 ||
          !read_ok || !received || *received != 11 || channel.length() != 0) {
        return -402;
      }
      if (thread::SelectUntil(absl::InfinitePast(),
                              {channel.writer()->OnWrite(std::move(losing))}) !=
              0 ||
          losing || channel.length() != 1) {
        return -403;
      }
      if (thread::Select({channel.reader()->OnRead(&received, &read_ok)}) !=
              0 ||
          !read_ok || !received || *received != 23 || channel.length() != 0) {
        return -404;
      }
      channel.writer()->Close();
      if (thread::Select({channel.reader()->OnRead(&received, &read_ok)}) !=
              0 ||
          read_ok) {
        return -405;
      }
    }
    {
      thread::Channel<int> channel(0);
      int received = 0;
      bool read_ok = false;
      int read_selection = -2;
      thread::Fiber receiver(executor.fibers(), [&] {
        read_selection = thread::SelectUntil(
            absl::Now() + absl::Seconds(2),
            {channel.reader()->OnRead(&received, &read_ok)});
      });
      if (!executor.DispatchReady().ok() || receiver.Finished()) {
        return -404;
      }
      int sent = 47;
      if (thread::SelectUntil(absl::InfinitePast(),
                              {channel.writer()->OnWrite(std::move(sent))}) !=
          0) {
        return -405;
      }
      for (int turn = 0; turn < 128 && !receiver.Finished(); ++turn) {
        if (!executor.DispatchReady().ok()) {
          return -406;
        }
      }
      if (!receiver.Finished() || read_selection != 0 || !read_ok ||
          received != 47 || channel.length() != 0) {
        return -407;
      }
      int absent = 61;
      if (thread::SelectUntil(absl::InfinitePast(),
                              {channel.writer()->OnWrite(std::move(absent))}) !=
              -1 ||
          absent != 61) {
        return -408;
      }
      channel.writer()->Close();
    }
    {
      thread::Channel<int> first(0);
      thread::Channel<int> second(0);
      int first_value = 0;
      int second_value = 0;
      bool first_ok = false;
      bool second_ok = false;
      int selected = -2;
      thread::Fiber receiver(executor.fibers(), [&] {
        selected = thread::SelectUntil(
            absl::Now() + absl::Seconds(2),
            {first.reader()->OnRead(&first_value, &first_ok),
             second.reader()->OnRead(&second_value, &second_ok)});
      });
      if (!executor.DispatchReady().ok() || receiver.Finished()) {
        return -409;
      }
      second.writer()->Write(73);
      for (int turn = 0; turn < 128 && !receiver.Finished(); ++turn) {
        if (!executor.DispatchReady().ok()) {
          return -411;
        }
      }
      if (!receiver.Finished() || selected != 1 || !second_ok ||
          second_value != 73 || first_ok || first_value != 0) {
        return -412;
      }
      first.writer()->Close();
      second.writer()->Close();
    }
    {
      thread::Channel<std::unique_ptr<int>> channel(1);
      channel.writer()->Write(std::make_unique<int>(5));
      std::unique_ptr<int> pending = std::make_unique<int>(7);
      bool written = true;
      thread::Fiber writer(executor.fibers(), [&] {
        written = channel.writer()->WriteUnlessCancelled(std::move(pending));
      });
      if (!executor.DispatchReady().ok() || writer.Finished()) {
        return -425;
      }
      writer.Cancel();
      for (int turn = 0; turn < 128 && !writer.Finished(); ++turn) {
        if (!executor.DispatchReady().ok()) {
          return -426;
        }
      }
      if (!writer.Finished() || written || !pending || *pending != 7 ||
          channel.length() != 1) {
        return -427;
      }
      std::unique_ptr<int> received;
      if (!channel.reader()->Read(&received) || !received || *received != 5) {
        return -420;
      }
      channel.writer()->Close();
    }
    {
      thread::Channel<std::unique_ptr<int>> channel(0);
      std::unique_ptr<int> pending = std::make_unique<int>(31);
      std::unique_ptr<int> received;
      bool read_ok = false;
      for (int attempt = 0; attempt < 200; ++attempt) {
        if (thread::SelectUntil(
                absl::InfinitePast(),
                {channel.reader()->OnRead(&received, &read_ok),
                 channel.writer()->OnWrite(std::move(pending))}) != -1 ||
            !pending || *pending != 31 || received || read_ok) {
          return -423;
        }
      }
      channel.writer()->Close();
    }
    {
      thread::Channel<int> channel(0);
      int selected = -2;
      int received = 0;
      bool read_ok = false;
      thread::Fiber receiver(executor.fibers(), [&] {
        selected = thread::SelectUntil(
            absl::Now() + absl::Milliseconds(5),
            {channel.reader()->OnRead(&received, &read_ok)});
      });
      for (int turn = 0; turn < 128 && !receiver.Finished(); ++turn) {
        if (!executor.DispatchReady().ok()) {
          return -428;
        }
        if (!receiver.Finished()) {
          executor.Park();
        }
      }
      if (!receiver.Finished() || selected != -1 || read_ok || received != 0 ||
          thread::SelectUntil(absl::InfinitePast(),
                              {channel.writer()->OnWrite(9)}) != -1) {
        return -429;
      }
      channel.writer()->Close();
    }
    {
      symbian::concurrency::Promise<int> source;
      auto computed = source.future().ThenOnWorker(
          executor,
          [event_thread](
              const absl::StatusOr<int>& value) -> absl::StatusOr<int> {
            if (std::this_thread::get_id() == event_thread || !value.ok()) {
              return absl::InternalError("Stackless worker placement failed");
            }
            return *value + 1;
          });
      auto shared_worker = executor.workers();
      if (!shared_worker.ok()) {
        return -390;
      }
      auto& worker = **shared_worker;
      auto fiber_task = worker.PostFiber([event_thread] {
        if (thread::Fiber::Current() == nullptr ||
            std::this_thread::get_id() == event_thread) {
          std::abort();
        }
        thread::Fiber::SleepFor(absl::Milliseconds(1));
      });
      source.SetValue(41);
      auto drained = worker.Finish();
      int answer = -1;
      thread::Fiber waiter(executor.fibers(), [&] {
        auto result = computed.Await(absl::Now() + absl::Seconds(2));
        if (result.ok()) {
          answer = *result;
        }
        if (!fiber_task.Await(absl::Now() + absl::Seconds(2)).ok() ||
            !drained.Await(absl::Now() + absl::Seconds(2)).ok()) {
          error = -382;
        }
      });
      for (int turn = 0; turn < 256 && !waiter.Finished(); ++turn) {
        if (!executor.DispatchReady().ok()) {
          return -383;
        }
        if (!waiter.Finished()) {
          executor.Park();
        }
      }
      if (!waiter.Finished() || answer != 42 || error != 0 ||
          !drained.IsReady()) {
        return -384;
      }
      if (worker.Post([] {}).code() != absl::StatusCode::kFailedPrecondition) {
        return -385;
      }
      auto rejected = symbian::concurrency::ReadyFuture(1).ThenOnWorker(
          executor,
          [](const absl::StatusOr<int>& value) -> absl::StatusOr<int> {
            return *value;
          });
      if (!rejected.IsReady() || rejected.ResultIfReady()->status().code() !=
                                     absl::StatusCode::kFailedPrecondition) {
        return -391;
      }
    }
    {
      symbian::concurrency::WorkerExecutor worker(1);
      thread::PermanentEvent release;
      auto blocked =
          worker.PostFiber([&] { thread::Select({release.OnEvent()}); });
      if (worker.Post([] {}).code() != absl::StatusCode::kResourceExhausted) {
        return -386;
      }
      release.Notify();
      auto drained = worker.Finish();
      thread::Fiber waiter(executor.fibers(), [&] {
        if (!blocked.Await(absl::Now() + absl::Seconds(2)).ok() ||
            !drained.Await(absl::Now() + absl::Seconds(2)).ok()) {
          error = -387;
        }
      });
      for (int turn = 0; turn < 256 && !waiter.Finished(); ++turn) {
        if (!executor.DispatchReady().ok()) {
          return -388;
        }
        if (!waiter.Finished()) {
          executor.Park();
        }
      }
      if (!waiter.Finished() || error != 0) {
        return -389;
      }
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
