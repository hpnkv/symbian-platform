# Failure report task lifecycle probe

Build from the source workspace:

```sh
cmake --preset debug
cmake --build --preset debug --target symbian_native_tool
cmake --preset guest-probes-armv6-release-gpu \
  -DSYMBIAN_NATIVE_CONVERTER="$PWD/build/debug/cpp/symbian/tools/symbian-native"
cmake --build --preset guest-probes-armv6-release-gpu \
  --target failure_handler_probe_e32 failure_handler_app_e32
SYMBIAN_FAILURE_HANDLER_WORKSPACE="$PWD" \
  .venv/bin/pytest -q symbian/tests/test_failure_handler_guest.py
```

The opt-in integration tests copy the preserved RM-807 instance into temporary
directories and use the controlled emulator on Dynarmic and Dyncom. Override
`SYMBIAN_FAILURE_HANDLER_INSTANCE` and `SYMBIAN_FAILURE_HANDLER_EMULATOR` to
select those prepared inputs. No physical device is accessed.

The probe uses the production failure-task coordinator and SDK storage,
window, fiber and worker utilities. It verifies background persistence and
persistence after an injected presentation error, then shows a green window
while its writer is suspended. Closing that window lets the writer proceed:
it deliberately waits three seconds without a window before writing and
flushing the final report. The test checks the live process, captures and file
contents before and after that interval. The actual CHECK-based failure demo
also renders, closes and retains its report before its expected exit reason 1.

Host GTest coverage in `symbian_failure_report_tasks_test` verifies that either
task may finish or fail first without cancelling the other, including joining
from an SDK fiber and a native caller. These tests establish lifecycle behavior,
not a physical-phone latency prediction.
