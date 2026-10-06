#include <apgcli.h>

int main() {
  RApaLsSession session;
  const TInt result = session.Connect();
  if (result != KErrNone) {
    return 1;
  }
  session.Close();
  return 0;
}
