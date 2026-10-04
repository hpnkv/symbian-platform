#include <memory>
#include <thread>

#include "window_server.h"

namespace {

struct RunState {
  int exit_code = 0;
};

}  // namespace

extern "C" int GuiMain() {
  // The worker owns the Window Server session, request statuses and event
  // dispatcher. The joining startup thread shares only its final result.
  auto run = std::make_shared<RunState>();
  std::thread event_thread([run] { run->exit_code = GuiWindowServerMain(); });
  event_thread.join();
  return run->exit_code;
}
