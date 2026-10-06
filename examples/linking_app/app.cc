#include <e32debug.h>

#include "functions.h"

int main() {
  const int result = LinkingStaticValue();
  RDebug::Print(_L("Static + dynamic library result: %d"), result);
  return result == 42 ? 0 : KErrCorrupt;
}
