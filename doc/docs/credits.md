# Projects behind this platform

Symbian development needs more than a compiler. This repository joins modern
host tools with preserved platform contracts and community work. These
projects remain independently maintained; source snapshots and adaptations
keep their original license notices.

## Running and understanding Symbian

[EKA2L1](https://github.com/EKA2L1/EKA2L1) is the community emulator used in
the SDK’s disposable application runs. It loads Symbian executables and, when
separately supplied, firmware and system files. Emulator behavior can differ
from a physical phone.

[SymbianSource](https://github.com/SymbianSource) preserves original Symbian
source, including kernel and user-library headers, EABI export definitions and
historical build tools. These are references for the SDK's selected import
proxies and for independent format checks. The original licenses are retained
in copied or exported material.

[SymbianRevive](https://github.com/SymbianRevive) develops modern Symbian
build tooling. The SDK's application-registration resource compiler is built
from a pinned [symbian-build](https://github.com/SymbianRevive/symbian-build)
source checkout with a recorded host adaptation. That checkout remains
outside the repository.

## C++ runtime and TLS

[Mbed TLS](https://github.com/Mbed-TLS/mbedtls) supplies the cryptographic and
TLS implementation. The Symbian adaptation began with
[shinovon's mbedtls-symbian](https://github.com/shinovon/mbedtls-symbian).
The complete source, headers, build project and tests are now vendored under
[`third_party/mbedtls-symbian`](https://github.com/hpnkv/symbian-platform/tree/main/third_party/mbedtls-symbian),
with original Apache-2.0 notices. The SDK builds the app-linked
archives from that local tree and adds no shared CA trust store.

[A11](https://github.com/hpnkv/a11) is the implementation reference for
status, concurrency and asynchronous ownership. Pinned, licensed source is
kept under `third_party/a11` and the concurrency component. The documentation
uses A11's Material and Doxygen visual settings, with copied style licenses.
[Abseil](https://github.com/abseil/abseil-cpp) supplies selected C++ libraries;
[LLVM](https://github.com/llvm/llvm-project) supplies Clang, LLD and libc++
source used by the modern host and guest toolchains. Source builds use pinned revisions and explicit target patches.

## Firmware and device material

Firmware, ROM/Z files and private handset records are supplied separately.
They are never redistributed in the SDK or this Git repository. The
[firmware guide](guides/firmware.md) explains how a developer selects local
material for emulator work.

For ROM and firmware resources, see EKA2L1's
[Important Links](https://eka2l1.miraheze.org/wiki/Important_Links) and the
[Delight firmware project](https://www.symwld.com/delight/), which maintains
community firmware for selected Symbian devices. Choose material for the
correct model and use it according to its distribution terms.

## Community resources

- [Symbian World](https://www.symwld.com/) collects Symbian applications,
  resources and community projects.
- [NNProject](https://nnproject.cc/) develops applications and utilities for
  Symbian and other mobile platforms.
- [Symbian World on Telegram](https://t.me/symbian_world) is a place for
  community discussion, support and news.
- [hstsethi's Awesome Symbian](https://github.com/hstsethi/awesome-symbian)
  curates links to Symbian software, tools and resources.
- [gauravssnl's Awesome Symbian](https://github.com/gauravssnl/awesome-symbian)
  collects applications, development resources and related projects.
