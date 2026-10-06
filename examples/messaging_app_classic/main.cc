#include <absl/base/nullability.h>
#include <msvapi.h>

namespace {

class SessionObserver final : public MMsvSessionObserver {
 public:
  void HandleSessionEventL(TMsvSessionEvent, TAny* absl_nullable,
                           TAny* absl_nullable,
                           TAny* absl_nullable) override {}
};

}  // namespace

int main() {
  CActiveScheduler scheduler;
  CActiveScheduler::Install(&scheduler);
  SessionObserver observer;
  CMsvSession* absl_nullable session = nullptr;
  TRAPD(error, session = CMsvSession::OpenSyncL(observer));
  delete session;
  CActiveScheduler::Install(nullptr);
  return error == KErrNone ? 0 : 1;
}
