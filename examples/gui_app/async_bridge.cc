#include "async_bridge.h"

#include <new>
#include <vector>

#include <absl/base/nullability.h>
#include <symbian/concurrency/event_executor.h>
#include <symbian/native_status.h>

struct GuiAsync {
  symbian::concurrency::EventExecutor executor;
  std::vector<symbian::concurrency::Task> tasks;
  absl::Status dispatch_status;
  unsigned int generation = 0;
  int due = 0;
};

GuiAsync* absl_nullable GuiAsyncCreate() {
  return new (std::nothrow) GuiAsync;
}

int GuiAsyncOpen(GuiAsync* absl_nonnull async) {
  return symbian::NativeErrorFromStatus(async->executor.Open());
}

void GuiAsyncSchedule(GuiAsync* absl_nonnull async) {
  const unsigned int generation = async->generation;
  auto task = async->executor.ScheduleAfter(absl::Milliseconds(300));
  task.OnReady([async, generation](const auto& result) {
    if (result.ok()) {
      if (const absl::Status queued =
              async->executor.DispatchToEvent([async, generation] {
                if (generation == async->generation) {
                  ++async->due;
                }
              });
          !queued.ok()) {
        async->dispatch_status = queued;
      }
    } else if (result.status().code() != absl::StatusCode::kCancelled) {
      async->dispatch_status = result.status();
    }
  });
  async->tasks.push_back(std::move(task));
}

void GuiAsyncCancel(GuiAsync* absl_nonnull async) {
  ++async->generation;
  async->due = 0;
  for (const auto& task : async->tasks) {
    task.Cancel();
  }
}

int GuiAsyncDispatch(GuiAsync* absl_nullable async) {
  if (const absl::Status dispatched = async->executor.DispatchReady();
      !dispatched.ok()) {
    return symbian::NativeErrorFromStatus(dispatched);
  }
  for (auto it = async->tasks.begin(); it != async->tasks.end();) {
    if (it->IsReady()) {
      it = async->tasks.erase(it);
    } else {
      ++it;
    }
  }
  if (!async->dispatch_status.ok()) {
    return symbian::NativeErrorFromStatus(async->dispatch_status);
  }
  const int due = async->due;
  async->due = 0;
  return due;
}

void GuiAsyncPark(GuiAsync* absl_nonnull async) {
  async->executor.Park();
}

void GuiAsyncDestroy(GuiAsync* absl_nullable async) {
  if (async == nullptr) {
    return;
  }
  GuiAsyncCancel(async);
  async->executor.Close();  // Drain before callbacks lose their state.
  delete async;
}
