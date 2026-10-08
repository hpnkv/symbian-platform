// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include <cstddef>
#include <cstdint>
#include <ctime>

#include <absl/base/nullability.h>

namespace {

// ESTLIB predates the tm_gmtoff and tm_zone fields used by Open C and cctz.
struct LegacyTm {
  int seconds;
  int minutes;
  int hours;
  int month_day;
  int month;
  int year;
  int week_day;
  int year_day;
  int daylight_saving;
};

extern "C" LegacyTm* absl_nullable symbian_estlib_gmtime_r(
    const std::time_t* absl_nonnull input, LegacyTm* absl_nonnull output);
extern "C" LegacyTm* absl_nullable symbian_estlib_localtime_r(
    const std::time_t* absl_nonnull input, LegacyTm* absl_nonnull output);
extern "C" std::time_t symbian_estlib_mktime(LegacyTm* absl_nonnull input);
extern "C" std::size_t symbian_estlib_strftime(
    char* absl_nonnull output, std::size_t capacity,
    const char* absl_nonnull format, const LegacyTm* absl_nonnull input);

char kUniversalZone[] = "UTC";
char kStandardZone[] = "STD";
char kDaylightZone[] = "DST";

LegacyTm ToLegacy(const std::tm& input) {
  return {input.tm_sec, input.tm_min, input.tm_hour,
          input.tm_mday, input.tm_mon, input.tm_year,
          input.tm_wday, input.tm_yday, input.tm_isdst};
}

void CopyFields(const LegacyTm& input, std::tm* absl_nonnull output) {
  output->tm_sec = input.seconds;
  output->tm_min = input.minutes;
  output->tm_hour = input.hours;
  output->tm_mday = input.month_day;
  output->tm_mon = input.month;
  output->tm_year = input.year;
  output->tm_wday = input.week_day;
  output->tm_yday = input.year_day;
  output->tm_isdst = input.daylight_saving;
}

std::int64_t DaysFromCivil(std::int64_t year, unsigned int month,
                           unsigned int day) {
  year -= month <= 2 ? 1 : 0;
  const std::int64_t era = (year >= 0 ? year : year - 399) / 400;
  const unsigned int year_of_era = static_cast<unsigned int>(year - era * 400);
  const unsigned int day_of_year =
      (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  const unsigned int year_of_era_day =
      year_of_era * 365 + year_of_era / 4 - year_of_era / 100 + day_of_year;
  return era * 146097 + static_cast<std::int64_t>(year_of_era_day) - 719468;
}

std::int64_t CivilSeconds(const LegacyTm& input) {
  const std::int64_t days =
      DaysFromCivil(static_cast<std::int64_t>(input.year) + 1900,
                    static_cast<unsigned int>(input.month + 1),
                    static_cast<unsigned int>(input.month_day));
  return days * 86400 + input.hours * 3600 + input.minutes * 60 +
         input.seconds;
}

void SetLocalFields(const LegacyTm& input, std::time_t epoch,
                    std::tm* absl_nonnull output) {
  CopyFields(input, output);
  output->tm_gmtoff = static_cast<long>(CivilSeconds(input) - epoch);
  output->tm_zone = input.daylight_saving > 0 ? kDaylightZone : kStandardZone;
}

}  // namespace

extern "C" std::tm* absl_nullable gmtime_r(
    const std::time_t* absl_nonnull input, std::tm* absl_nonnull output) {
  LegacyTm legacy = {};
  if (symbian_estlib_gmtime_r(input, &legacy) == nullptr) {
    return nullptr;
  }
  CopyFields(legacy, output);
  output->tm_gmtoff = 0;
  output->tm_zone = kUniversalZone;
  return output;
}

extern "C" std::tm* absl_nullable localtime_r(
    const std::time_t* absl_nonnull input, std::tm* absl_nonnull output) {
  LegacyTm legacy = {};
  if (symbian_estlib_localtime_r(input, &legacy) == nullptr) {
    return nullptr;
  }
  SetLocalFields(legacy, *input, output);
  return output;
}

extern "C" std::time_t mktime(std::tm* absl_nonnull input) {
  LegacyTm legacy = ToLegacy(*input);
  const std::time_t result = symbian_estlib_mktime(&legacy);
  if (result != static_cast<std::time_t>(-1)) {
    SetLocalFields(legacy, result, input);
  }
  return result;
}

extern "C" std::size_t strftime(char* absl_nonnull output,
                                 std::size_t capacity,
                                 const char* absl_nonnull format,
                                 const std::tm* absl_nonnull input) {
  const LegacyTm legacy = ToLegacy(*input);
  return symbian_estlib_strftime(output, capacity, format, &legacy);
}
