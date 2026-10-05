#include <e32std.h>

extern "C" int RuntimeCheckFinalizers();

// Keep the original OS header boundary separate from modern libc++ headers.
__attribute__((destructor)) void CheckFinalizerResult() {
  const int result = RuntimeCheckFinalizers();
  if (result != 0) {
    User::Exit(result);
  }
}
