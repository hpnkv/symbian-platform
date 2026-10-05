#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include <unistd.h>

namespace {

int Format(char* output, size_t capacity, const char* format, ...) {
  va_list arguments;
  va_start(arguments, format);
  const int result = vsnprintf(output, capacity, format, arguments);
  va_end(arguments);
  return result;
}

}  // namespace

int SymbianRuntimeVarargsProbe() {
  char output[64] = {};
  constexpr char expected[] = "value=-42, ok";
  const int count = Format(output, sizeof(output), "value=%d, %s", -42, "ok");
  if (count != sizeof(expected) - 1) {
    return -181;
  }
  for (size_t index = 0; index < sizeof(expected); ++index) {
    if (output[index] != expected[index]) {
      return -182;
    }
  }
  constexpr char stacked[] = "args=1/2/3/4/4294967297/done";
  const int stacked_count =
      Format(output, sizeof(output), "args=%d/%d/%d/%d/%lld/%s", 1, 2, 3, 4,
             4294967297LL, "done");
  if (stacked_count != sizeof(stacked) - 1) {
    return -184;
  }
  for (size_t index = 0; index < sizeof(stacked); ++index) {
    if (output[index] != stacked[index]) {
      return -185;
    }
  }
  char* end = nullptr;
  if (strtol("  -125rest", &end, 10) != -125 || strcmp(end, "rest") != 0 ||
      strtol("0x2a", &end, 0) != 42 || *end != '\0') {
    return -186;
  }
  char copy[12] = {};
  if (strcpy(copy, "c-service") != copy || strcmp(copy, "c-service") != 0) {
    return -187;
  }
  if (sysconf(_SC_PAGESIZE) <= 0) {
    return -188;
  }
  timespec pause{0, 1000000};
  if (nanosleep(&pause, nullptr) != 0) {
    return -190;
  }
#ifdef SYMBIAN_RUNTIME_CHANGED_VARARGS
  return -183;
#else
  return 0;
#endif
}
