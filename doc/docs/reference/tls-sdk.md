# Mbed TLS SDK integration

The [application TLS guide](../guides/tls.md) gives the project steps. This reference describes the packaged sources, targets and transport adapters.

This SDK builds its TLS libraries from the complete, locally vendored
[mbedtls-symbian](https://github.com/shinovon/mbedtls-symbian) source port,
which adapts the upstream [Mbed TLS](https://github.com/Mbed-TLS/mbedtls)
project for Symbian. Link the library only in applications that need it. A
TLS session uses a certificate to authenticate its peer and needs a trusted
entropy source for cryptographic randomness. The SDK supplies the OS secure
RNG on supported EABI systems; unsupported systems fail closed.

The default SDK export builds the vendored
`third_party/mbedtls-symbian` port for ARMv5T and ARMv6. The repository
contains its CMake project, source files, public and private headers, tests,
and original license notices. No sibling checkout is required. The SDK
installs Mbed TLS 3.4.1 headers, the static `mbedcrypto`, `mbedx509` and
`mbedtls` archives, architecture-specific CMake package targets, the
Apache-2.0 license, a complete inspectable source copy in
`source/mbedtls-symbian`. The SDK digest inventory covers installed payload
integrity.
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
requires compatible clock and libc services on the target. The port also
provides nonblocking OpenC socket BIO callbacks. For SDK HTTP and WebSockets,
use `TlsStream` over `Symbian::Connectivity`'s native `RSocket` transport;
OpenC outbound socket I/O is unavailable in the RM-807 emulator profile.

The SDK provides one shared ARMv5T/ARMv6 entropy provider for EABI ROMs that
export `Math::RandomL(TDes8&)` and `Math::Random(TDes8&)`. It resolves and
validates both native ROM wrappers and their common non-leaving secure RNG
veneer, preserving `KErrNotReady` instead of discarding it. The OS selects
the executive number; applications do not need a device-specific adapter.
Absent exports, RAM/unknown wrapper forms, partial output and native failures
produce `MBEDTLS_ERR_ENTROPY_SOURCE_FAILED`, with zero produced bytes and
cleared output. There is no time, jitter or `Math::Random()` fallback.

Original C7/E6 and 808 ROM wrappers share this contract with different syscall
numbers. The inspected 6120/E71 ROMs lack these exports; those systems need a
separately verified secure source before cryptographic sessions can work.
The current emulator matrix exercises five firmware fixtures on both ISAs
and both CPU backends. RM-807 succeeds with fresh buffers. C7/E6 wrappers
validate, but the tested emulator profile lacks their secure RNG syscall
(`0x109`); they fail closed with cleared output. 6120/E71 also fail closed.
The legacy OpenSSL compatibility adapter uses this same SDK provider and
credits entropy only after a complete successful fill.
The provider's firmware/emulator checks do not establish physical-device RNG
quality or compatibility. Native leave/unwind support is not required.
Certificate trust policy belongs to the application; no CA roots are loaded
implicitly.

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

### Socket BIO API

`symbian_mbedtls_socket_bio_attach` takes an already connected OpenC socket
descriptor and requests `O_NONBLOCK`. The caller retains ownership of that
descriptor and must close it after the TLS owner has stopped using the BIO.
Pass the BIO to `mbedtls_ssl_set_bio` with
`symbian_mbedtls_socket_bio_send` and
`symbian_mbedtls_socket_bio_recv`; retry `MBEDTLS_ERR_SSL_WANT_READ` and
`MBEDTLS_ERR_SSL_WANT_WRITE` only when the socket becomes ready. Call
`symbian_mbedtls_socket_bio_cancel` to make later callback invocations return
`MBEDTLS_ERR_NET_CONN_RESET`. The cancel flag does not interrupt a callback
already inside OpenC. For native SDK TLS sessions, use `TlsStream` over the native Socket Server
transport instead.
