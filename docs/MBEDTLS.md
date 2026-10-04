# Mbed TLS in the application SDK

The default SDK export builds the vendored
`third_party/mbedtls-symbian` port for ARMv5T and ARMv6. The repository
contains its CMake project, source files, public and private headers, tests,
and original license notices. No sibling checkout is required. The SDK
installs Mbed TLS 3.4.1 headers, the static `mbedcrypto`, `mbedx509` and
`mbedtls` archives, architecture-specific CMake package targets, the
Apache-2.0 license, a complete inspectable source copy in
`source/mbedtls-symbian`, and source/build provenance with digests.
Cloning an older installed SDK with `symbian sdk install` also fails with a
message to use `--workspace` for a fresh export; it cannot silently copy an
SDK without the default TLS package.

An application opts in through CMake:

```cmake
find_package(MbedTLS 3.4.1 EXACT CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE MbedTLS::mbedtls)
```

The SDK toolchain selects the architecture-matched package. The TLS target
brings in X.509 and crypto; applications needing only cryptography can link
`MbedTLS::mbedcrypto`. Headers are also available from the SDK's `include/`
root. No TLS archive is linked by ordinary application targets unless they
explicitly or transitively request it.

The packaged port compiles TLS 1.2 and TLS 1.3. These archives do not replace
Symbian's system `ssl.dll`. The port supplies a UTC adapter using the SDK's
clock and libc imports, with an invalid-clock failure path. Its guest behavior
remains to be tested. Guest applications still need a verified entropy source
and transport callbacks. Certificate trust policy
belongs to the application; the SDK does not silently install a CA bundle.
The project CMake option `SYMBIAN_CA_BUNDLE` selects a PEM file inside that
project. Leave it empty to package no roots. For example, configure with
`-DSYMBIAN_CA_BUNDLE:STRING=certs/private-ca.pem`. CMake checks the path and
256 KiB limit, prints its SHA-256, and exposes
`SYMBIAN_CA_BUNDLE_PACKAGED_PATH` to the application. The `symbian package`
step validates every PEM certificate, rejects other PEM blocks, and installs
the exact bytes at `\\resource\\apps\\<executable-stem>_ca.pem`. The package
report records the source, target and SHA-256. A bundle requires an
`[application]` registration; it changes no SDK trust store or Mbed TLS
defaults. The TLS owner must explicitly load these roots and require peer
verification. Packaging reads the selection from the ELF build directory's
`CMakeCache.txt` and rejects a cache belonging to another project. Explicit
key pinning remains possible without a CA bundle.
Authenticated host-side TLS tests and guest cryptography execution have been
reported by the port, but a guest TLS handshake and physical Nokia 808 TLS
connection remain acceptance gates. See [DEVELOPMENT_AGENT.md](../DEVELOPMENT_AGENT.md)
for the service rollout and [STATUS.md](STATUS.md) for current evidence.
