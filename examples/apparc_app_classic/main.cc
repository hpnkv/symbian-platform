#include <apgcli.h>

#include "ui.h"

int RunFeature(void* absl_nullable) {
  RApaLsSession session;
  const TInt result = session.Connect();
  if (result != KErrNone) {
    return 1;
  }
  session.Close();
  return 0;
}

int main() {
  return classic_demo_ui::Show(_L("APPLICATION SERVICES"),
                               _L("Connects to the AppArc server."),
                               _L("Checks that the session opens."),
                               _L("APPARC SESSION OPEN"), &RunFeature, nullptr);
}
