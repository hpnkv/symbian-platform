#include "app_bridge.h"

#include <new>

#include "model.h"

#if defined(SYMBIAN_ENABLE_ABSEIL_STATUS) || defined(SYMBIAN_ENABLE_TIMER_TASKS)
#include <symbian/native_status.h>
#endif

#ifdef SYMBIAN_ENABLE_TIMER_TASKS
#include <vector>

#include <symbian/concurrency/event_executor.h>

namespace {

struct AppState {
  AppModel model;
  symbian::concurrency::EventExecutor executor;
  std::vector<symbian::concurrency::Task> tasks;
  absl::Status dispatch_status;
  int due = 0;
  unsigned int generation = 0;
};

AppModel* Model(void* app) {
  return &static_cast<AppState*>(app)->model;
}

const AppModel* Model(const void* app) {
  return &static_cast<const AppState*>(app)->model;
}

}  // namespace

extern "C" int AppTasksOpen(void* app) {
  auto* state = static_cast<AppState*>(app);
  return symbian::NativeErrorFromStatus(state->executor.Open());
}

extern "C" void AppTasksSchedule(void* app) {
  auto* state = static_cast<AppState*>(app);
  const unsigned int generation = state->generation;
  auto task = state->executor.ScheduleAfter(absl::Milliseconds(1500));
  task.OnReady([state, generation](const auto& result) {
    if (result.ok()) {
      const absl::Status queued =
          state->executor.DispatchToEvent([state, generation] {
            if (generation == state->generation) {
              ++state->due;
            }
          });
      if (!queued.ok()) {
        state->dispatch_status = queued;
      }
    } else if (result.status().code() != absl::StatusCode::kCancelled) {
      state->dispatch_status = result.status();
    }
  });
  state->tasks.push_back(std::move(task));
}

extern "C" void AppTasksCancel(void* app) {
  auto* state = static_cast<AppState*>(app);
  ++state->generation;
  state->due = 0;
  for (const auto& task : state->tasks) {
    task.Cancel();
  }
}

extern "C" int AppTasksDispatch(void* app) {
  auto* state = static_cast<AppState*>(app);
  const absl::Status dispatched = state->executor.DispatchReady();
  if (!dispatched.ok()) {
    return symbian::NativeErrorFromStatus(dispatched);
  }
  auto& tasks = state->tasks;
  for (auto it = tasks.begin(); it != tasks.end();) {
    if (it->IsReady()) {
      it = tasks.erase(it);
    } else {
      ++it;
    }
  }
  const int due = state->due;
  state->due = 0;
  if (!state->dispatch_status.ok()) {
    return symbian::NativeErrorFromStatus(state->dispatch_status);
  }
  return due;
}

extern "C" void AppTasksPark(void* app) {
  static_cast<AppState*>(app)->executor.Park();
}
#else
namespace {
AppModel* Model(void* app) {
  return static_cast<AppModel*>(app);
}

const AppModel* Model(const void* app) {
  return static_cast<const AppModel*>(app);
}
}  // namespace
#endif

extern "C" void* AppCreate() {
#ifdef SYMBIAN_ENABLE_TIMER_TASKS
  return new (std::nothrow) AppState;
#else
  return new (std::nothrow) AppModel;
#endif
}

extern "C" void AppDestroy(void* app) {
#ifdef SYMBIAN_ENABLE_TIMER_TASKS
  auto* state = static_cast<AppState*>(app);
  if (state != nullptr) {
    AppTasksCancel(state);
    // OnReady may run inline as Close drains native statuses. Keep the
    // result Status and model alive until that drainage finishes.
    state->executor.Close();
  }
  delete state;
#else
  delete static_cast<AppModel*>(app);
#endif
}

extern "C" int AppLogTime(void* app, ClockTime now) {
#ifdef SYMBIAN_ENABLE_ABSEIL_STATUS
  return symbian::NativeErrorFromStatus(Model(app)->LogTime(now));
#else
  Model(app)->LogTime(now);
  return 0;
#endif
}

extern "C" void AppClear(void* app) {
  Model(app)->Clear();
}

extern "C" int AppLineCount(const void* app) {
  return Model(app)->LineCount();
}

extern "C" const char* AppLine(const void* app, int line) {
#ifdef SYMBIAN_ENABLE_ABSEIL_STATUS
  auto value = Model(app)->Line(line);
  return value.ok() ? *value : nullptr;
#else
  return Model(app)->Line(line);
#endif
}
