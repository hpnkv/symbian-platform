# Mbed TLS SDK integration

The [application TLS guide](../guides/tls.md) gives the project steps. This reference records source, build and current runtime evidence.

This SDK builds its TLS libraries from the complete, locally vendored
[mbedtls-symbian](https://github.com/shinovon/mbedtls-symbian) source port,
which adapts the upstream [Mbed TLS](https://github.com/Mbed-TLS/mbedtls)
project for Symbian. Link the library only in applications that need it. A
TLS session uses a certificate to authenticate its peer and needs a trusted
entropy source for cryptographic randomness. The default guest entropy
callback fails closed until a target-specific source is supplied.

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
has been exercised in a dynamic DLL on both emulator backends. The source port
also includes nonblocking socket BIO callbacks with cancellation; host tests
pass, and an emulator DLL probe verifies that cancellation stops later send and
receive callbacks before they touch the socket. A separate opt-in RM-807
emulator patch and DLL probe show a connected TCP receive returning
`MBEDTLS_ERR_SSL_WANT_READ` when empty, delivering a delayed byte and refusing
a read after cancellation. The OpenC `send` and `sendto` entry points do not
provide working outbound I/O in this emulator profile. The separate
`Symbian::Connectivity` target uses the original `RSocket` API and completes
native send and receive. An opt-in TLS research DLL links that client with an
RM-807 entropy adapter; it completed authenticated TLS 1.2 and TLS 1.3
handshakes and exchanged application data in disposable emulator instances.
The standard SDK archive still needs a verified secure entropy source for its
actual target. Certificate trust policy
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
Authenticated host-side TLS 1.2/1.3 tests pass. The opt-in guest TLS DLL
checks both protocol versions with a local OpenSSL server, peer verification,
application data, wrong-host, untrusted-certificate and expired-certificate
rejection in each protocol version. A C++ owner with
cancellable operations and a physical Nokia 808 TLS connection remain gates.

An opt-in RM-807 emulator research probe now reaches Belle EUSER's secure
random executive call through an ARM-state adapter. A local EKA2L1 patch
supplies that call from libuv's host OS random source, and the DLL probe passed
on both emulator CPU backends. The normal SDK archive continues to fail closed
for guest entropy. This experiment does not validate entropy on a phone. See
[the agent plan](https://github.com/hpnkv/symbian-platform/blob/main/.dev/development-agent.md)
for the service rollout and [STATUS.md](https://github.com/hpnkv/symbian-platform/blob/main/.dev/status.md) for current evidence.

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
already inside OpenC. The opt-in emulator experiment covers receive and
post-cancel behavior only; it is not yet a supported connected TLS transport.
