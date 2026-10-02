#include <__config>
#include <exception>

#if _LIBCPP_HAS_EXCEPTIONS
#error This exception-count adapter requires a no-exceptions runtime.
#endif

namespace std {
int uncaught_exceptions() noexcept {
  return 0;
}
}  // namespace std
