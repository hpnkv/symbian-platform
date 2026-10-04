# Add Mbed TLS to an application

The SDK includes the complete source and headers of the
[mbedtls-symbian](https://github.com/shinovon/mbedtls-symbian) port, based on
[Mbed TLS](https://github.com/Mbed-TLS/mbedtls). Your project links the
installed ARM library targets. TLS needs cryptographic randomness and a
trusted certificate chain; the guest handshake remains an open gate.

## 1. Link the library

In the application's `CMakeLists.txt`:

```cmake
find_package(MbedTLS 3.4.1 EXACT CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE MbedTLS::mbedtls)
```

The TLS target includes X.509 and crypto dependencies. For a hash-only use,
link `MbedTLS::mbedcrypto` instead. An ordinary project does not link TLS
unless it selects one of these targets.

## 2. Choose trust for this project

If the app will use CA roots, put a PEM bundle inside its project and configure:

```sh
cmake --preset symbian-pic -DSYMBIAN_CA_BUNDLE:STRING=certs/roots.pem
```

Run this from a generated application project. Its `symbian-pic` preset
selects the SDK and build directory. The option packages the PEM into this app only; it does not
alter SDK-wide or device trust. The application must load the packaged roots,
request peer verification and check the hostname. Leaving the option empty
packages no CA roots. The [TLS SDK reference](../reference/tls-sdk.md) covers
path checks, package placement and exact current test evidence.

## 3. Interpret the result

The installed archives compile TLS 1.2 and 1.3 and selected guest crypto,
clock and certificate checks run in the emulator. Guest entropy is currently
fail-closed and connected socket I/O has an unresolved nonblocking-contract
failure, so an authenticated guest handshake is not yet demonstrated. Do not
ship a security claim based on an ARM build or a host handshake. Follow the
[development status](https://github.com/hpnkv/symbian-platform/blob/main/.dev/status.md)
for the next verified gate.
