# Mbed TLS for the modern Symbian SDK

This fork brings shinovon’s Mbed TLS 3.4.1 Symbian port under the modern
Clang/CMake SDK. The initial deliverable is application-linked static libraries;
it does not replace the phone’s system `ssl.dll`.

The SDK is under active development. Build and execution evidence, required
platform services and limitations are recorded in the owner's private
`~/.symbian-dev/status.md` archive.

Cryptographic sources remain in `library/` and public headers in `include/`.
Original licenses and copyright notices remain intact. Historical MMP, SIS and
frozen-export files are preserved under `legacy/symbian/` for reference, outside
the new build. `docs/UPSTREAM_README.md` preserves the inherited documentation.

See [provenance](docs/PROVENANCE.md) for the exact fork point.

## Build

Select a materialized SDK with `sdk-location.json` (ignored):

```json
{"sdk": "../symbian-sdk"}
```

Or pass `-DSYMBIAN_SDK_PREFIX=/path/to/sdk`; the environment variable of the
same name is accepted. A current SDK export from the local workspace is required.
The older visible SDK may not contain the ARMv6 runtime or static-library helper.

```sh
cmake --preset symbian-debug
cmake --build --preset symbian-debug
cmake --install build/symbian-debug --prefix install/armv6
```

`symbian-armv5t` is the alternate ISA preset. Native verification uses
`host-debug`, requires GTest and Abseil, and runs with `ctest --preset host-debug`.

Consumers use `find_package(MbedTLS 3.4.1 EXACT CONFIG REQUIRED)` and link
`MbedTLS::mbedtls`, `MbedTLS::mbedx509` or `MbedTLS::mbedcrypto`. The consuming
project selects its own SDK; installed targets propagate the matching configuration.

The SDK supplies verified OS entropy, bounded heap and UTC adapters; applications
supply trust policy and transport ownership. Unsupported secure RNG contracts
fail closed. Acceptance executes AES/SHA through a guest DLL and authenticated
TLS 1.2/1.3 client/server sessions in the emulator as well as on the host.
Physical-phone compatibility remains unverified. Owned port headers compile
independently in C and C++ through `symbian_header_canaries`.
