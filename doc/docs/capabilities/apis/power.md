# Power component

**Implemented:** `Symbian::Power` exports `ReadPowerSnapshot()` and
`ResetInactivityTimer()` in
`<symbian/api/power/power.h>`.

## Motivation and modernization

The original HAL exposes separately queried integer attributes and a small
battery-status enumeration. A caller cannot safely derive a charge percentage
or a charging state from those integers. `PowerSnapshot` uses three independent
`std::optional` fields: power-good, external supply in use, and a typed
`BatteryCondition`. A partial result remains useful without inventing values
for unsupported attributes. `absl::StatusOr` reports when none can be read.

## Ownership and cost

`native_power.cc` owns the legacy HAL header and performs three synchronous
queries. The modern adapter maps only documented values; unknown integer
values stay unknown. The snapshot owns no native handle and allocates no
buffers. It is read on demand, without caching dynamic power state. A caller
that needs a bounded event callback should issue the query from a worker.

## Decide whether to postpone background sync

Link `Symbian::Power`. This policy postpones work when the battery is low,
empty or needs replacement, unless an external supply is known to be present.
An unknown battery produces an unknown decision instead of a guessed percentage.

```cpp
#include "symbian/api/power/power.h"

absl::StatusOr<std::optional<bool>> ShouldPostponeSync() {
  auto power = symbian::api::power::ReadPowerSnapshot();
  if (!power.ok()) {
    return power.status();
  }
  if (power->external_power == true) {
    return std::optional<bool>{false};
  }
  if (!power->battery) {
    return std::optional<bool>{};
  }
  return std::optional<bool>{*power->battery !=
                             symbian::api::power::BatteryCondition::kGood};
}
```

The caller decides how to handle an unknown result. External supply here affects
work policy only; it does not imply that the battery is charging.

## Restrictions

The snapshot is not atomic across its three HAL calls. Change subscriptions
and model-specific charging information are unavailable. External power alone
is never labelled "charging".

`ResetInactivityTimer()` restarts the system display inactivity timer without
changing the user's timeout setting. An app with a continuously visible task
can call it about once per second while its window is foreground, then stop
calling it on focus loss. `examples/condition_display` uses this for its color
cycle.
