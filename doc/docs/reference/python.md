# Python API reference

The CLI and desktop console are the usual entry points. These modules expose
the status and SDK model used by applications and build tooling.

## Status values

The platform transports native errors as A11 `Status` values and exposes
Python exceptions through `symbian.status`. CLI results use the same status
codes in their JSON output. See the [console guide](../guides/console.md)
for its in-process status boundary.

## Application SDK

::: symbian.project.sdk.AppSdk
