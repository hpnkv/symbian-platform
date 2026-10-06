#include <uri8.h>

#include "ui.h"

_LIT8(KExampleUri, "https://example.org/path?q=1");
_LIT8(KExpectedHost, "example.org");

int RunFeature(void* absl_nullable) {
  TUriParser8 uri;
  if (uri.Parse(KExampleUri) != KErrNone) {
    return 1;
  }
  return uri.Extract(EUriHost).Compare(KExpectedHost) == 0 ? 0 : 2;
}

int main() {
  return classic_demo_ui::Show(
      _L("URI SERVICES"), _L("Parses an HTTPS address."),
      _L("Extracts its host name."), _L("HTTPS HOST EXTRACTED"), &RunFeature,
      nullptr);
}
