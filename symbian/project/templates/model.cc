#include "model.h"

namespace {

void Pair(std::string& text, int position, int value) {
  text[static_cast<std::size_t>(position)] =
      static_cast<char>('0' + value / 10);
  text[static_cast<std::size_t>(position + 1)] =
      static_cast<char>('0' + value % 10);
}

}  // namespace

#ifdef SYMBIAN_ENABLE_ABSEIL_STATUS
absl::Status AppModel::LogTime(ClockTime now) {
  if (now.month < 1 || now.month > 12 || now.day < 1 || now.day > 31 ||
      now.hour < 0 || now.hour > 23 || now.minute < 0 || now.minute > 59 ||
      now.second < 0 || now.second > 59) {
    return absl::InvalidArgumentError("Clock fields are out of range");
  }
#else
void AppModel::LogTime(ClockTime now) {
#endif
  // Keep memory and the visible log bounded.
  if (lines_.size() == 12) {
    lines_.erase(lines_.begin());
  }
  std::string text = "0000-00-00 00:00:00";
  Pair(text, 0, now.year / 100);
  Pair(text, 2, now.year % 100);
  Pair(text, 5, now.month);
  Pair(text, 8, now.day);
  Pair(text, 11, now.hour);
  Pair(text, 14, now.minute);
  Pair(text, 17, now.second);
  lines_.push_back(std::move(text));
#ifdef SYMBIAN_ENABLE_ABSEIL_STATUS
  return absl::OkStatus();
#endif
}

void AppModel::Clear() {
  lines_.clear();
}

int AppModel::LineCount() const {
  return static_cast<int>(lines_.size());
}

#ifdef SYMBIAN_ENABLE_ABSEIL_STATUS
absl::StatusOr<const char*> AppModel::Line(int index) const {
  if (index < 0 || index >= LineCount()) {
    return absl::OutOfRangeError("Log line index is out of range");
  }
#else
const char* AppModel::Line(int index) const {
#endif
  return lines_[static_cast<std::size_t>(index)].c_str();
}
