#include <absl/base/nullability.h>
#include <badesca.h>
#include <calsession.h>

int main() {
  CActiveScheduler scheduler;
  CActiveScheduler::Install(&scheduler);
  CCalSession* absl_nullable session = nullptr;
  TRAPD(open_error, session = CCalSession::NewL());
  if (open_error != KErrNone) {
    CActiveScheduler::Install(nullptr);
    return 1;
  }
  CDesCArray* absl_nullable files = nullptr;
  TRAPD(list_error, files = session->ListCalFilesL());
  delete files;
  delete session;
  CActiveScheduler::Install(nullptr);
  return list_error == KErrNone ? 0 : 2;
}
