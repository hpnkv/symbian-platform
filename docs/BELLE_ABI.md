# RM-807 executive ABI experiment

The supplied Delight 1.8 image now executes the GUI's initial drawing function
in the patched macOS EKA2L1 frontend. This is a bounded firmware experiment:
rendered pixels, pointer delivery, normal exit and complete Belle support are
still unverified. The physical phone has not been accessed.

The original ZIP declares RM-807, product 059M7Q4 and firmware 113.010.1508.
Its imported `platform.txt` declares SymbianOSMajorVersion=101. The pinned
emulator's software-version detector collapses 100 and 101 into `epoc10`;
that enum alone cannot select the correct executive call numbers.

## Source and ROM comparison

The opt-in native `symbian_rom_abi_probe` uses the unchanged EKA2L1 ROM parsers
to inspect the preserved input and maps a private ROM copy. It executes no
guest instructions. The actual EUSER has code at `0x804bcce8`, 301,100 code
bytes, 2,566 exports and image version 2.4.0. Its export addresses are read
from the mapped ROM, rather than treating the small Z-drive ROM header as a
standalone executable.

The original kernel's
[executive definition](https://github.com/SymbianSource/oss.FCL.sf.os.kernelhwsrv/blob/0c3208650587ac0230aed8a74e9bddb5288023eb/kernel/eka/kernel/execs.txt)
and generator identify source call numbers. The original EUSER frozen DEF
provides function names/ordinals. Joining those with the actual ROM exports
allows LLVM to create a local disassembly container with analysis labels.
That ELF is neither a guest executable nor recovered firmware debug symbols.
GDB decodes the real instruction bytes; Python research scripts compare text,
source wrappers and call sets, without implementing a ROM binary parser.

The retained analysis contains 313 source-wrapper constraints, 212 distinct
old-call mappings and no conflicting constraints. This is research evidence,
with ambiguous or unmatched wrappers left out. It is not a complete semantic
specification of every private executive operation. Observations support the
following piecewise routing, rather than a uniform shift:

| Original call range | Observed firmware range |
| --- | --- |
| `0x00..0x10` | Same numbers |
| `0x11..0xD5` | Original number + 2 |
| `0xD8..0x10B` | Original number + 1 |
| Observed fast calls, starting at `0x800000` | Same numbers |

For example HAL `0x4F` becomes `0x51`, chunk creation `0x6B` becomes `0x6D`,
thread kill `0x73` becomes `0x75`, thread-exiting `0xF6` becomes `0xF7`, and
static-call completion `0x10B` becomes `0x10C`. Live stops independently
confirm the HAL/chunk arguments and successful heap setup with this routing.
The identities of the new `0x11/0x12` calls and the intervening private
`0xD6/0xD7` source operations remain unresolved. No claim identifies which
private operation was removed.

The GPL `symbian101-experimental.patch` builds a separate map from existing
handlers and retains the original `svc_register_funcs_v10`. The later ordered
`belle-library-entry-start.patch` maps the observed 0x10D hook. A bounded SDK
C++ DLL constructor now runs through the real process-attach list on both
backends/architectures; detach, TLS and complete DLL initialization remain
unverified. Genuine firmware SVC `0xFF` can also conflict with EKA2L1's
existing HLE trampoline interception; that collision is unresolved.

## Explicit selection and limits

Enable the experiment only on a disposable copy of the imported baseline:

```sh
EKA2L1_DATA_ROOT="$PWD/.symbian/instances/your-fresh-copy" \
EKA2L1_EXPERIMENTAL_SVC_PROFILE=rm807-113.010.1508 \
  "$SYMBIAN_EMULATOR" --device RM-807 --run 'C:\sys\bin\gui_app.exe'
```

The profile requires `epoc10` and the exact full-ROM SHA-256
`b5c1ea63cb6359270c5b7cfb1bb453594e208a01b8aeb5b5e020f37d546f7086`.
Unknown profile names and altered ROM bytes are rejected before profile
selection. It is not an automatic rule for every Belle image. The digest guard
does not authenticate the ZIP, ROFS contents or physical-phone identity.
Without the environment variable the original map remains selected and the
GUI's heap setup still returns -1; that control is retained in the live tests.

Apply the six patches in WALKTHROUGH.md section 6 to the pinned checkout.
`guest-debug-library-query.patch` removes advertisement of an unsupported
GDB library-list query. This permits GDB's normal `file` command followed by
relocated `symbol-file`; dynamic library introspection remains unsupported.

## ARM register and cleanup-stack corrections

The first routed run reaches `GuiMain`, but Dynarmic aborts on actual EUSER
instruction `0xee1d0f70` at `0x804c26f0`: `mrc p15,0,r0,c13,c0,3`.
This reads TPIDRURO, which is separate from the existing TPIDRURW TLS register.
The [Arm architecture manual](https://documentation-service.arm.com/static/5f8daeb7f86e16515cdb8c4e)
defines that register and its user read-only access. The original
[Symbian SMP scheduler](https://github.com/SymbianSource/oss.FCL.sf.os.kernelhwsrv/blob/0c3208650587ac0230aed8a74e9bddb5288023eb/kernel/eka/nkernsmp/arm/ncsched.cia)
saves/restores both thread registers independently; its initialization zeros
them. The interpreter already implements the read but did not preserve that
register in a thread context.

`guest-thread-register.patch` adds a separate context value, the Dynarmic read,
and save/restore on both tested macOS backends, with zero for a new HLE thread.
Four native cases execute real unprivileged ARM reads with unequal values,
switch contexts, restore a saved context and check that TLS is not aliased.
This patch does not implement writes to TPIDRURO or claim ARM32/12l1r support.

The next run completes `RWsSession::Connect()` with zero, then panics with
`E32USER-CBase/69` during screen-device construction. The original
[panic definition](https://github.com/SymbianSource/oss.FCL.sf.os.kernelhwsrv/blob/0c3208650587ac0230aed8a74e9bddb5288023eb/kernel/eka/include/e32panic.h)
identifies a missing trap handler. The example now uses the real SDK
`CTrapCleanup::New()` before `GuiMain`, handles allocation failure, and deletes
it when `GuiMain` returns. The extra EUSER import is frozen ordinal 196.
This supplies the cleanup stack needed by SDK calls; it does not enable C++
exceptions or prove the example's normal cleanup/exit path.

With these changes a live source breakpoint reaches `DrawGui` at `0x700003ae`.
GDB inspects count=0, running=true and the actual 360 by 640 layout, then
`finish` returns to `0x700002f0` after the drawing function's guest calls.
Those calls may be buffered. No framebuffer capture or pointer delivery is
established by this stop, and the owned frontend is forcibly stopped afterward.

## Replay and private evidence

```sh
cmake --build build/eka2l1 --target eka2l1_qt \
  symbian_rom_abi_probe symbian_svc_profile_probe symbian_cpu_register_probe
ctest --test-dir build/eka2l1 \
  -R '^symbian_(svc_profile|cpu_register)_probe$' --output-on-failure
SYMBIAN_ROM_ABI_ROOT="$PWD/.symbian/instances/delight-import-01" \
SYMBIAN_ROM_ABI_OUTPUT="$PWD/.symbian/belle-abi-research/new-euser-analysis" \
SYMBIAN_E32_TEST_IMAGE="$PWD/.symbian/e32-probe/e32_probe.exe" \
  build/eka2l1/platform-tests/symbian_rom_abi_probe
SYMBIAN_GUI_DEBUG_GOLDEN_ROOT="$PWD/.symbian/instances/delight-import-01" \
SYMBIAN_GUI_DEBUG_BUILD="$PWD/.symbian/gui-app" \
SYMBIAN_EKA2L1_EXECUTABLE="$SYMBIAN_EMULATOR" \
  uv run pytest -q symbian/tests/test_guest_debugger.py \
    --basetemp .symbian/belle-abi-research/new-debug-test
```

Both output directories must be fresh; Pytest clears its basetemp. The ROM
probe rejects existing output and outputs inside/containing the preserved root.
The live tests use copied states, digest checks and owned process cleanup;
they cover both default and experimental routing and two real rejection cases.

Private evidence is under `.symbian/belle-abi-research`: native ROM exports and
code, generated executive definitions, source-wrapper mappings, disassembly,
routing/register GTest JSON, live GDB/instance logs, ROM output controls and
patch replay. The earlier `delight-profile-02/04` runs retain the coprocessor
abort, `delight-profile-06` retains panic 69, and the cleanup run retains the
initial drawing-function stop. Firmware, derived code, instances and build
products remain excluded from Git and the production wheel.


## Render/input/exit follow-up

The guarded profile now has separate end-to-end evidence on Dynarmic and Dyncom:
real screen-texture captures, two increment taps, an outside-control tap, reset
and normal SDK exit with reason zero. The frontend exits zero and retains a
native kernel process-exit report. This advances the earlier drawing-call-only
checkpoint; later DLL attach tests resolve the observed 0x10D mapping, but not
SVC 0xFF, full DLL lifetime, complete
unwinding or general firmware compatibility. Replay and bounds are in
[EMULATOR_CONTROL.md](EMULATOR_CONTROL.md).
