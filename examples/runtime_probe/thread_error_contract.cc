#include <thread>

// In the default no-exceptions profile, joining an empty std::thread must
// enter libc++'s fatal error path. A normal return is a failed control.
extern "C" int SymbianRuntimeThreadErrorProbe() {
  std::thread empty;
  empty.join();
  return -243;
}
