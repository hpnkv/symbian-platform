# EKA1 support and limits

Implemented and exercised on 2026-10-05: the first capability scoped by
[eka1-plan.md](eka1-plan.md), a freestanding **no-UI ARMv5T process** on preserved
Nokia 7610 RH-51 / Symbian OS 8.0 firmware. Both Dynarmic and Dyncom run it and
record a normal process exit with reason 7610. A separately built version
returns 7611 on both backends; checking it against 7610 fails as intended.
This is emulator process support, not general EKA1 application/device support.
The next bounded slice also executes five original EUSER imports: allocation,
allocation length/cell counts, copying and freeing. Both backends restore the
original heap counts; changed-result and corrupted-copy controls pass.

## Use the profile

From the prepared source worktree, with Clang/LLD on PATH:

```sh
python -m symbian.cli build --project examples/eka1_probe \
  --output .symbian/eka1-build
python -m symbian.cli toolchain verify-eka1 \
  .symbian/eka1-build/eka1_probe.exe \
  --firmware 7610 --store .symbian/firmware-store \
  --backend dynarmic --output .symbian/eka1-dynarmic
python -m symbian.cli toolchain verify-eka1 \
  .symbian/eka1-build/eka1_probe.exe \
  --firmware 7610 --store .symbian/firmware-store \
  --backend dyncom --output .symbian/eka1-dyncom
```

Use the alias assigned to your import, or the exact `sha256:` identity below.
The verifier accepts only the tested fixture; another 7610 revision and P900
remain unverified. `--emulator PATH` selects the maintained frontend explicitly.
The output directory must be new and outside preserved firmware. Results retain
image/frontend digests, firmware identity, native exits, frontend exit, logs
and the disposable instance. A nonmatching reason or missing exit fails even
when the frontend exits zero. `--expected-reason N` changes the exit oracle.

The complete example lives in `examples/eka1_probe/`, using ordinary CMake
presets/Ninja, with a compile database and two-tree reproducibility checks.
Its entire application function is:

```cpp
extern "C" int Eka1Main() { return 7610; }
```

Its separate ARM entry preserves the return address, calls the Thumb-1 function
and returns its result in r0. It has no EKA2 startup marker, direct executive
calls, EUSER proxies or runtime library. A new project must declare
`kind = "e32-eka1"`, `architecture = "armv5t"`, and a compatible callable ARM
entry/linker layout. Do not reuse the EKA2 SDK startup.

To replay acceptance, including changed-result controls:

```sh
SYMBIAN_EKA1_GUEST=1 python -m pytest \
  symbian/tests/test_eka1.py symbian/tests/test_eka1_guest.py -v
SYMBIAN_EKA1_TEST_IMAGE="$PWD/.symbian/eka1-build/eka1_probe.exe" \
  build/eka2l1/platform-tests/symbian_e32_oracle \
  '--gtest_filter=Eka1OracleTest.*'
```

## Original EUSER imports

The complete `examples/eka1_import_probe/` uses `kind = "e32-eka1-import"`
and a selected legacy-ordinal proxy. Its DEF records only symbol/ordinal facts;
The research log records the pinned ordinal source and original EUSER digest.
Follow the [user guide](../doc/docs/guides/eka1.md) to build the proxy, build the
process and run it on either backend. Modern Belle EUSER proxies are incompatible.

The exercised boundary has GNU2 symbol names and explicit integer/pointer
declarations in `legacy_euser.h`: `User::AllocLen` (39), `User::AllocSize` (41),
`User::Alloc` (45), `Mem::Copy` (243) and `User::Free` (476). This is not a
general C++ ABI adapter. `Mem::Copy` returns the end pointer, `dest + length`.
The probe checks 64 copied bytes, live heap counts and exact restoration after
free. Allocation failure returns -4; failures 40–43 identify length/end-pointer,
copied-data, live-count and cleanup errors. The corrupt-copy control returns
41 after freeing its allocation.

Native conversion reuses validated ELF proxy resolution and retained PLT call
relocations. The final `.got.plt` function slots become a contiguous PE import
address table at `text_size`; a zero word terminates it. The legacy import
section contains ordinals, whereas EKA2 ELF import sections contain code offsets.
Native inspection checks the section against every IAT word and its terminator.
Only one function-only `euser.dll` block is currently supported.

```sh
SYMBIAN_EKA1_GUEST=1 python -m pytest \
  symbian/tests/test_eka1_import.py -v
SYMBIAN_EKA1_IMPORT_TEST_IMAGE="$PWD/.symbian/eka1-import-build/eka1_import_probe.exe" \
  build/eka2l1/platform-tests/symbian_e32_oracle \
  '--gtest_filter=Eka1OracleTest.IndependentlyChecksPeImportsAndIat'
```

## Format and entry contract

Native `ConvertEka1Executable` shares the existing retained-relocation ELF
validation, then publishes a bounded legacy E32 image. `InspectImage` reports
`kernel = "eka1"`; EKA2 metadata reports `"eka2"`. Python orchestrates builds
and copies, with all format handling remaining native.

| Field | EKA1 process profile | Existing EKA2 profile |
| --- | --- | --- |
| Header size | 124 bytes | 156-byte minimum V header |
| Offset 20 | Legacy ARM CPU `0x2000` | Header CRC |
| Offsets 24/28 | Code/data checksums | Module version/compression |
| Flags | 0, original header, no EABI/EKA2/ELF-import flags | `0x12000028` |
| Entry | ARM function that returns an integer | EKA2 marker and thread/process startup |
| SID/capabilities | Absent, reported as zero | V security extension |
| Imports/relocations | Optional contiguous EUSER PE IAT; no fixups | Validated eager ordinal imports/typed fixups |

The probe is one RX mapping with position-independent internal branches.
Unsupported data/BSS, absolute pointer fixups, other imports, exports, lifecycle
arrays and exception metadata are rejected. The example explicitly excludes
Clang's cantunwind-only `.ARM.exidx` metadata; it does not provide unwinding.
Input must be trusted and retain relocations: handwritten absolute addresses
and stripped relocation history cannot be proved safe by this converter.
The native checker validates the generated profile's additive word checksum.
The independent emulator parser checks identity and bounds, but **does not
establish historical loader checksum validation**. The original EKA2/V
validator is not an EKA1 validation oracle.

Clang emits EABI5 objects as an intermediate format. Simple integer return,
internal ARM/Thumb interworking and the exercised entry work; this is not
proof that AAPCS C++ classes, mangled symbols, descriptors, floating-point or
leaves match the historical EKA1 ABI. No legacy compiler was introduced.

## Firmware and bootstrap evidence

- Firmware content identity:
  `80c85c43e74cd6f6bc2a071e32d2efdebe1fa0a3c5352c08217327b6415bfbe3`.
- ROM SHA-256:
  `a5a2b1fb499410ced638ad590edc7dfbfcd491839919d1a11cf58f76c64aee9a`.
- Original EUSER SHA-256:
  `7ae4317ffb1fc4f29506439bc6a21392fe9dd9bfdfd75c07a9727ed677cabd51`.
- Initial process image SHA-256:
  `64238a00231c929bf395e1534476d0b615fa2e51dbedc92df460df4236bf9589`.
- Imported heap/copy process SHA-256:
  `8accf68f01299e6eed6f4da5c8cb819e503b63fb8e215e6e498f0edcf63b85fc`.
- Executed frontend SHA-256:
  `c32f93f1b67921ca8c7dd1933fc099eb860d876f68eddaeda281a133d3e97982`.

The frontend uses pinned EKA2L1 revision
`2594edf4d6bf55d7bd3f0b46250fe2318d4dc2e8` plus the existing maintained patches
and local research changes. No EKA1 emulator shim/source change was needed;
these results describe the identified local binary, not pristine upstream.

EKA2L1's existing `thread::reset_thread_ctx` and
`lib_manager::build_eka1_thread_bootstrap_code` create/switch the heap using
original EUSER ordinals 166 and 1127, invoke DLL entries, call the image entry,
and forward r0 to original EUSER ordinal 397 (`Thread::Exit`). That emulator
bootstrap is part of the tested contract; bypassing it or copying the image to
a physical phone requires an independent startup/loader gate.

The preserved original `instrec.exe` is a **ROM image**, not a downloadable
E32 executable: UID1 `0x1000007a`, flags 2, priority 350, Thumb entry
`0x50adc225`, code address `0x50adc224`, no writable data/BSS, and a ROM DLL
reference table. Its startup is not the EKA2 marker. ROM headers/import
references differ from disk E32/PE import tables; do not infer that modern
EUSER ordinals/proxies are compatible. Original header/entry observations
are retained privately in `.symbian/eka1-20261005/original-rom-contract.json`.

## Restrictions and remaining gates

- Only the named Nokia 7610 fixture and both tested macOS emulator backends.
  P900 import remains available, but its process ABI has not been exercised.
- No Window Server/font/UI application profile, generated GUI app, normal
  Console app Run/Debug, AppArc registration or general OS boot claim.
- No general historical C++ import ABI, SDK runtime, libc++, heap-owning C++
  objects, global/static lifecycle, DLL publishing, threads or concurrency.
  The five explicit EUSER calls above are the tested heap/import boundary.
- No native HTTP/WebSocket/TLS on EKA1 yet: those use the separately tested
  EKA2 runtime, socket services and entropy contract.
- No EKA1 SIS installation/signing. The native SISX builder explicitly rejects
  EKA1 images because EKA1 requires the older SIS format.
- No general emulator debugging/source-unwind or physical-device claim.

The firmware store is checked before and after every run. Only disposable state
is writable; emulator startup may modify its copied Z files. All baseline
hashes matched after acceptance. Owner-writable file permissions still do not
replace an independently held offline preservation copy.

Next gates are broader original ABI calls, writable data/relocations and
lifecycle, then UI
and legacy packaging with their own positive/negative controls. Do not relax
these restrictions because an ARM ELF builds or a header parses.
