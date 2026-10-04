# Add Mbed TLS to an application

The SDK includes the complete source and headers of the
[mbedtls-symbian](https://github.com/shinovon/mbedtls-symbian) port, based on
[Mbed TLS](https://github.com/Mbed-TLS/mbedtls). Your project links the
installed ARM library targets. TLS needs cryptographic randomness and a
trusted certificate chain. An opt-in emulator experiment has completed
authenticated handshakes; the standard guest entropy source still fails
closed, so applications must supply one for their actual target.

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

The installed archives compile TLS 1.2 and 1.3. In a patched, disposable
RM-807 emulator instance, a research DLL using native `RSocket` completed
authenticated client handshakes and exchanged application data with a local
server using each protocol. Wrong hostname and untrusted certificate controls
failed verification. The opt-in entropy adapter and emulator patch are not
the default SDK configuration; cancellation, listener ownership and a
physical-device result remain open. Follow the
[development status](https://github.com/hpnkv/symbian-platform/blob/main/.dev/status.md)
for the next verified gate.
