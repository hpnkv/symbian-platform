# Work to resume after the 2026-10-06 session

The user requested a partial-state handoff and an end to this session. Do not
interpret this checkpoint as completion of the platform mission. Read plan.md,
research.md, status.md and the latest research-log.md entries before resuming.

## Release the current changes

- VERSION remains 0.1.7. Increment the version and publish a fresh SDK release
  after checking the committed changes in hosted CI. Never move published tags.
- Verify the new source-workspace regression in Native SDK CI. It currently
  runs on native-guest cache misses; make its cache/invalidation independent
  enough to also cover root CMake/preset-only changes without rebuilding an
  unchanged native SDK payload. Then check the complete
  four-host native/archive matrix and Python 3.11–3.14 wheels/sdist publication.
  No release for this session's asset and CMake changes has been started.
- Repeat the installed Getting started, hello_time, gui_app and qt_app guide
  acceptance against the new public artefacts. SDK 0.1.7 already passed these
  workflows on macOS arm64 and Linux x86_64; that does not validate a future
  release. Emulator 0.1.0 is independently published and needs no new release
  unless its sources or SDK compatibility actually change.

## Finish the source-workspace developer experience

- Root guest CMake graphs now use repository headers/imports and source-built
  runtime, Abseil, Mbed TLS and device APIs. Host GUI builds use a separate guest
  graph. The source-workspace check covers configure/reload/build for both guest
  architectures and the host GUI; keep extending it as targets are added.
- Audit source-guide CLI and the existing GUI Run/Debug supervisor workflows:
  those currently retain their standalone installed-SDK build path. Decide how
  to run/debug a root CMake-produced E32 and its matching ELF directly, including
  registration resource compilation, rather than rebuilding with an installed
  SDK. Do not confuse the fixed source CMake link graph with this follow-up.
- Update gui-build.md and source-prerequisites.md with the root source-build
  route; the CLion root-workspace guides have already been updated. Keep
  standalone application instructions based on installed distributions.
- Check actual IDE navigation/Run/Remote Debug after those changes. The user's
  screenshots confirmed two profiles loading; the third had a corrupted
  generated compiler file and its debug cache was regenerated. Command-line
  configure/build checks do not prove all GUI debugger interactions.
- Expand the source CMake checks into a maintained inventory of standalone
  examples/probes/library projects and asset-placement regressions, beyond the
  current root graphs and existing distribution relocation tests.

## Remove remaining obsolete SDK conventions

- Finish the SDK-only compatibility audit. AsyncUsbSession still collects
  `_legacy_completions` and exposes `take_legacy_completions`; inspect the native
  transfer-ID and Future API contracts before removing that machinery coherently.
- Audit remaining old helper aliases, root-level compatibility runtime archive
  exports and stale documentation/utility assumptions. Keep legitimate fallbacks
  and historical Symbian formats, APIs, EKA1 and ARMv5T support. The user authorized
  removal of this SDK's own obsolete interfaces, not historical platform support.

## Complete broader outstanding streams

- Original Symbian headers: finish full class/member/function/parameter/enum/
  constant descriptions using live hosted sources where available. The previous
  audit found 712 of 5,395 indexed public/protected members without descriptions;
  rerun it before work. Put generated/source suffixes after brief and details,
  once per fully generated class where appropriate. Public C++ snippets must
  use the repository clang-format style.
- Begin the repository-wide mutable-reference-to-pointer/nullability revision.
  The emulator distribution and guest Qt example prerequisites are delivered;
  the revision itself has not started. Use mutable `T* absl_nonnull` arguments,
  and apply absl_nullable/absl_nullability_unknown consistently, adjusting bodies,
  callers, bindings and tests. Retain standard-required signatures where needed.
- Continue EKA1 beyond the documented bounded process/import support: broader
  runtime, original API and GUI/Qt restrictions need explicit implementation and
  execution checks. Record restrictions and actual results in .dev/EKA1.md.
- Linux arm64 emulator/runtime acceptance, full guest debugger/IDE unwinding and
  physical-device execution remain unverified. Use helena@192.168.1.209 with
  repository root ~/dev/symbian-platform for Linux x86_64 validation. Its working
  tree is an older checkout overlaid with newer sources: do not reset/pull it
  blindly; retain local research, fixtures and source backups.

## Local validation inputs

- Source-workspace inputs are generated at .symbian/workspace-inputs and excluded
  from Git. Machine user state defaults to ~/.symbian; old locations are ignored.
- Linux LLVM 23 is at .symbian/toolchains/LLVM-23.1.2-Linux-X64/bin. Its raw tools
  need LD_LIBRARY_PATH=.symbian/toolchains/icu70/usr/lib/x86_64-linux-gnu; host
  dependencies use .symbian/host-deps. These are repository-local prepared tool
  dependencies, not an installed native SDK selection.
- Source-workspace validation deliberately supplies nonexistent installed SDK
  paths. Logs are in /tmp/symbian-workspace-regression-macos.log and, on Linux,
  /tmp/symbian-workspace-regression-linux.log; the final Linux host follow-up is
  /tmp/symbian-workspace-linux-host-final.log on macOS.

## Cleanup completed at handoff

Pre-today scratch builds/files in .symbian/ and build/ were removed. Usable SDK,
firmware/import, upstream research and hosted documentation inputs remain, along
with current-day outputs and active debug/guest-probes-armv6 IDE trees. Old
nested diagnostic runs and proxy CMake caches were also removed. The independent
session's ongoing changes were left untouched; do not reset the worktree when
resuming. Workspace publication now safely replaces stale destination symlinks
without modifying their source targets.
