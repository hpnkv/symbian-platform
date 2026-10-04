# Native application-registration resource compiler

The resource compiler is the original EPL-1.0 Symbian `rcomp` from
`SymbianRevive/symbian-build` at
`d3c2eadd3ff7826bdf9e1d92f447c357571af18b` (its internal Symbian
baseline is `0f5e3a7fb6af`). The prepared source checkout is kept at
`.symbian/rcomp-epl-research` and is not versioned here. Its source retains
its Nokia/Symbian copyright and EPL headers. `modern-host.patch` is the
ordered adaptation for modern 64-bit macOS/Linux compilers: pointer width,
file-name diagnostics and registration of source locations. The compatibility
headers in `compat/` only map obsolete host C++ include names to current
standard headers. The SDK's companion `uidcrc` uses the independent native
UID checksum implementation in `cpp/symbian/analysis/checksum.h`.

Builds must check the pin and `git apply --reverse --check` before using an
already adapted checkout, or check/apply the patch against the clean pin.
The host binaries are built during a prepared-workspace SDK export. The
installed SDK contains `bin/rcomp`, `bin/uidcrc`, the genuine `AppInfo.rh`,
and the EPL-1.0 notice. Existing SDK exports need rebuilding to gain this
facility. Registered SIS output is verified by the native SIS inspector.
