// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "stdtime/timelocal.h"

#include <absl/base/nullability.h>

// The older runtime exposes the C locale. Keep the original Open C parser and
// supply its locale data without importing the newer process locale service.
extern "C" lc_time_T* absl_nonnull __get_current_time_locale() {
  static lc_time_T locale = {
      .mon = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep",
              "Oct", "Nov", "Dec"},
      .month = {"January", "February", "March", "April", "May", "June", "July",
                "August", "September", "October", "November", "December"},
      .wday = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"},
      .weekday = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday",
                  "Friday", "Saturday"},
      .X_fmt = "%H:%M:%S",
      .x_fmt = "%m/%d/%y",
      .c_fmt = "%a %b %e %H:%M:%S %Y",
      .am = "AM",
      .pm = "PM",
      .date_fmt = "%a %b %e %H:%M:%S %Z %Y",
      .alt_month = {"January", "February", "March", "April", "May", "June",
                    "July", "August", "September", "October", "November",
                    "December"},
      .md_order = "md",
      .ampm_fmt = "%I:%M:%S %p"};
  return &locale;
}
