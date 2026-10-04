# Sensors component

**Planned, not exported:** this directory has no archive or public header yet.

## Motivation and proposed modernization

The candidate Sensor Server APIs use channel handles, callbacks and
service-specific data packages. Discovery should produce typed channel
descriptors with units and supported rates. A move-only subscription would
own its channel and return timestamped sample values; unknown channels would
remain discoverable rather than being mislabelled as a known sensor type.
`absl::Time` would carry sample times and `absl::Status` would carry service
and permission errors.

## Concurrency and cost

Samples can outpace application processing. The event thread should copy only
the bounded native sample into an owned buffer, then publish through a bounded
queue with an explicit drop policy and drop count. Computation on samples
belongs on a worker. A long-lived channel avoids reopen cost for each sample,
and cancellation must stop and drain callbacks before buffer release.

## Evidence before implementation

Verify the native channel API, data layout, rates, capability requirements,
timestamps, overflow behavior and cancellation on a named device or firmware
profile. None of those details is assumed from the directory layout.
