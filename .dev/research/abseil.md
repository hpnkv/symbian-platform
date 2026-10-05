# Guest Abseil source and SDK export

A11 pins the original Apache-2.0 Abseil source at
`5650e9cf76d3be4318d5fa3af38ee483ddfd5e4a` (Abseil 20260526).
The clean checkout lives outside Git at `research/upstream/abseil-cpp`.
The exported SDK records that revision, the source license, the three ordered
patch digests, header/archive digests and the compiled target architectures.
Consumers of an installed SDK do not need the source checkout.

The exporter clones the clean pin, applies these patches in order, and checks
that each reverses from the replayed tree:

1. `symbian-platform.patch` narrows ELF-loader, Fuchsia timezone, `O_CLOEXEC`
   and demangler assumptions; uses Abseil's existing `GetTID()` fallback
   instead of unsupported E32 ELF TLS; and limits a byte-atomic compiler
   lock-free assertion to platforms where it describes the compiler accurately.
   The SDK supplies real EUSER-backed byte atomics on ARMv5T and ARMv6.
2. `symbian-low-level-alloc.patch` retains Abseil's allocator, arena and
   skiplist logic, while obtaining page-aligned process-owned memory from the
   SDK's `RChunk` bridge. Owner metadata is released with the chunk. This
   page source is currently tested for same-thread use; async-signal-safe
   arenas are unavailable because the bridge opens handles and allocates
   metadata.
3. `symbian-container-no-elf-tls.patch` changes the raw-hash-table seed's
   unsupported static ELF TLS into a process-wide atomic sequence. This is a
   bounded E32 adaptation, not general C++ `thread_local` support.

To prepare a checkout without A11's local dependency cache, obtain the pinned
revision from Abseil's upstream Git repository and leave its tracked files
clean. Verify patch replay before an SDK export:

```sh
git clone https://github.com/abseil/abseil-cpp.git research/upstream/abseil-cpp
git -C research/upstream/abseil-cpp checkout \
  5650e9cf76d3be4318d5fa3af38ee483ddfd5e4a
git -C research/upstream/abseil-cpp rev-parse HEAD
git -C research/upstream/abseil-cpp status --short
git -C research/upstream/abseil-cpp apply --check \
  "$PWD/research/abseil/symbian-platform.patch"
```

`symbian sdk install DEST --workspace "$PWD"` builds the selected pinned
closure twice, for ARMv5T and ARMv6. It installs the original headers under
`include/abseil`, 43 archives per architecture under `lib/<arch>/abseil`,
the Apache-2.0 notice and `Symbian::AbseilStatusOr` CMake interface target.
That target supplies the tested Status/StatusOr, Cord-payload and
`flat_hash_map` closure with the matching stream-runtime profile and selected
OS import proxies. The tracked `probes/abseil_status_probe` supports both
source replay and installed-SDK modes. A copied project built and converted
with no source checkout on its target include/link paths.

The maintained RM-807 matrix passed normal and deliberately changed-result
guest execution from source (8/8) and from a sealed SDK (8/8), covering
ARMv5T/ARMv6 and Dyncom/Dynarmic. The separate `LowLevelAlloc` contract also
passed 2/2. See `docs/RESEARCH_LOG.md` for rejected approaches and test
details. This establishes a useful guest subset, not every Abseil API.
Cross-thread page ownership, broader synchronization, general ELF TLS,
Abseil time and A11 fibers remain open.
