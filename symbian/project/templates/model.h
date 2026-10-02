#ifndef APP_MODEL_H_
#define APP_MODEL_H_

#include <string>
#include <vector>

#ifdef SYMBIAN_ENABLE_ABSEIL_STATUS
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#endif

#include "clock_time.h"

// Edit this ordinary C++ class for application state and behavior. The SDK's
// app_bridge.cc owns the C ABI boundary required by the original GUI headers.
class AppModel {
 public:
#ifdef SYMBIAN_ENABLE_ABSEIL_STATUS
  absl::Status LogTime(ClockTime now);
#else
  void LogTime(ClockTime now);
#endif
  void Clear();
  int LineCount() const;
#ifdef SYMBIAN_ENABLE_ABSEIL_STATUS
  absl::StatusOr<const char*> Line(int index) const;
#else
  const char* Line(int index) const;
#endif

 private:
  std::vector<std::string> lines_;
};

#endif  // APP_MODEL_H_
