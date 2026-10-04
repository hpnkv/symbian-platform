# EKA1 application support: smallest useful slice

Status: plan only. Do not enable EKA1 Run, Debug or application generation from
this document.

## Why this is separate

The current SDK builds ARM EABI/E32-V applications for EKA2. The firmware
importer already recognizes EKA1 dumps, including the tested 7610 and P900
examples, but Run rejects an EKA2 starter before creating an instance. The
EKA1 loader requires a different startup/import contract. A successful EKA2
build or firmware import says nothing about that contract.

## First capability

Add **one no-UI EKA1 process probe** for one explicitly named, preserved
emulator firmware fixture. Its only observable work is entry, a fixed result,
and normal exit recorded by EKA2L1's process-exit report. Keep the EKA2
application path, default SDK runtime, project generator and Console EKA1
disablement as they are until this probe passes. Do not promise Window Server,
SIS installation, modern libc++, Mbed TLS or phone compatibility in this slice.

## Research and implementation sequence

1. Record the selected firmware identity, EUSER digest, emulator revision and
   the original EKA1 executable's E32 header, imports, entry and startup
   behavior. Compare those facts with the current EKA2 publisher. Use the
   pinned EKA2L1 loader and original Symbian source as references, and keep
   any private firmware bytes outside Git.
2. Write a bounded ABI note: image/header differences, compiler calling
   convention, entry sequence, EUSER bootstrap, import encoding and expected
   process exit. If Clang cannot produce the required ABI, stop and report the
   gap before adding a historical compiler or runtime dependency.
3. Add an opt-in EKA1 build profile using the existing Clang/LLD, native E32
   converter, CMake/Ninja and firmware resolver wherever their contracts
   match. Place any necessary format handling in the native E32 library and
   isolate EKA1 startup code from the EKA2 runtime. Add no scheduler, crypto,
   networking, Java or Qt dependency for the probe.
4. Validate the artifact independently, then run it in a disposable instance
   of the named EKA1 firmware. Require a recorded process exit with the
   expected reason on Dynarmic and Dyncom, plus a deliberately changed-result
   control. Keep import/image and emulator evidence separate.
5. After this gate passes, expose only that tested profile in CLI output. Keep
   generated GUI apps and Console Run disabled for EKA1 until their own entry,
   UI, resource, packaging and cleanup gates pass.

## Acceptance and stop rules

The first slice is accepted only when the named-fixture process actually runs
and exits on both emulator CPU backends, its import and image metadata match
the recorded ABI, the negative control fails as expected, and existing EKA2
tests remain green. A linked ARM ELF, valid-looking E32 or EKA1 firmware import
alone is insufficient. No physical device claim follows from the emulator.

If the probe needs a new compiler, exception runtime, EUSER replacement,
emulator-wide compatibility shim or source-tree fork, pause the capability
expansion and record the smallest demonstrated blocker in the research log.
Prefer an isolated, reviewable patch to broad platform emulation.
