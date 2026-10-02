#include <cerrno>
#include <string>
#include <system_error>

int SymbianRuntimeSystemErrorProbe() {
  const std::error_code code =
      std::make_error_code(std::errc::invalid_argument);
  if (code.value() != EINVAL || code.category() != std::generic_category()) {
    return -186;
  }
  const std::string message = code.message();
  if (message.empty()) {
    return -187;
  }
  const std::error_condition condition = code.default_error_condition();
  if (condition != std::errc::invalid_argument) {
    return -188;
  }
#ifdef SYMBIAN_RUNTIME_CHANGED_SYSTEM_ERROR
  return -189;
#else
  return 0;
#endif
}
