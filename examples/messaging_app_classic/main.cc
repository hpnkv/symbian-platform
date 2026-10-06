#include <absl/base/nullability.h>
#include <msvapi.h>

#include "ui.h"

namespace {

class SessionObserver final : public MMsvSessionObserver {
 public:
  void HandleSessionEventL(TMsvSessionEvent, TAny* absl_nullable,
                           TAny* absl_nullable, TAny* absl_nullable) override {}
};

}  // namespace

int RunFeature(void* absl_nullable) {
  CActiveScheduler scheduler;
  CActiveScheduler::Install(&scheduler);
  SessionObserver observer;
  CMsvSession* absl_nullable session = nullptr;
  TRAPD(error, session = CMsvSession::OpenSyncL(observer));
  delete session;
  CActiveScheduler::Install(nullptr);
  return error == KErrNone ? 0 : 1;
}

int main() {
  return classic_demo_ui::Show(
      _L("MESSAGE SERVER"), _L("Opens the message server."),
      _L("Checks the session is ready."), _L("MESSAGE SESSION OPEN"),
      &RunFeature, nullptr);
}
