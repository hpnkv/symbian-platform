#include <uri8.h>

_LIT8(KExampleUri, "https://example.org/path?q=1");
_LIT8(KExpectedHost, "example.org");

int main() {
  TUriParser8 uri;
  if (uri.Parse(KExampleUri) != KErrNone) {
    return 1;
  }
  return uri.Extract(EUriHost).Compare(KExpectedHost) == 0 ? 0 : 2;
}
