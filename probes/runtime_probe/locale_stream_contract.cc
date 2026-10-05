#include <locale>
#include <sstream>
#include <string>

int SymbianRuntimeLocaleStreamProbe() {
  std::ostringstream output;
  output << "value " << 42;
#ifdef SYMBIAN_RUNTIME_CHANGED_LOCALE_STREAM
  if (output.str() != "value 41") {
    return -194;
  }
#else
  if (output.str() != "value 42") {
    return -194;
  }
#endif
  const std::locale classic("C");
  const std::locale posix("POSIX");
  if (classic.name() != "C" || posix.name() != "POSIX") {
    return -195;
  }
  return 0;
}
