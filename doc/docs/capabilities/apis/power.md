# Power component

**Implemented:** `Symbian::Power` exports `ReadPowerSnapshot()` in
`<symbian/api/power/power.h>`.

## Motivation and modernization

The original HAL exposes separately queried integer attributes and a small
battery-status enumeration. A caller cannot safely derive a charge percentage
or a charging state from those integers. `PowerSnapshot` uses three independent
`std::optional` fields: power-good, external supply in use, and a typed
`BatteryCondition`. A partial result remains useful without inventing values
for unsupported attributes. `absl::StatusOr` reports when none can be read.

## Boundary and cost

`native_power.cc` owns the legacy HAL header and performs three synchronous
queries. The modern adapter maps only documented values; unknown integer
values stay unknown. The snapshot owns no native handle and allocates no
buffers. It is read on demand, without caching dynamic power state. A caller
that needs a bounded event callback should issue the query from a worker.

## Remaining work

The snapshot is not atomic across its three HAL calls. Subscriptions,
model-specific charging information and physical-phone validation need their
own native contracts and tests. External power alone is never labelled
"charging".
