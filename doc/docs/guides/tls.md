# Use TLS with HTTP and WebSockets

The SDK ships the Mbed TLS 3.4.1 Symbian port and a native `TlsStream`. Link
`Symbian::Tls` with `Symbian::Http` for HTTPS, or with `Symbian::WebSocket` for
secure RFC 8441 WebSockets:

```cmake
target_link_libraries(my_app PRIVATE Symbian::Http Symbian::Tls Symbian::Storage)
```

`TlsStream::Connect` requires explicit PEM trust roots, the expected DNS hostname,
one TLS version, and an ALPN protocol. It sends SNI and verifies certificate
chain, dates and hostname. `kTls12` selects TLS 1.2 and `kTls13` selects TLS 1.3;
there is no silent version fallback or verification bypass. Use ALPN `http/1.1`
with `Protocol::kHttp11`, or `h2` with `Protocol::kHttp2`. HTTP/2 requires the peer
to select `h2`; HTTP/1.1 also permits a server that omits ALPN.

All operations and destruction belong to one SDK worker, outside an active
scheduler callback. Absolute deadlines cover the native BIO's socket I/O.
The SDK supplies entropy through the OS secure RNG on supported EABI ROMs,
using one provider on ARMv5T and ARMv6. Applications need no entropy adapter
there. Older systems without that API and unsupported native wrappers fail
closed; see [entropy contracts](../reference/tls-sdk.md) before targeting them.
UTC comes from the runtime and certificate-date checking remains enabled.

## Package and load a CA bundle

Put selected public CA certificates in `certs/roots.pem` inside a generated
application named `my_app`, then configure:

```sh
cmake --preset symbian-pic -DSYMBIAN_CA_BUNDLE:STRING=certs/roots.pem
```

The SDK packages `\resource\apps\my_app_ca.pem` into this application's SIS and
records its SHA-256. It changes no device-wide trust store. The caller must
load the PEM explicitly. This complete helper loads a bounded file, handling
short reads and truncation:

```cpp
#include <span>
#include <string>

#include "symbian/api/storage/storage.h"

absl::StatusOr<std::string> LoadPem(std::u16string_view path) {
  auto file = symbian::api::storage::ReadOnlyFile::Open(path);
  if (!file.ok()) {
    return file.status();
  }
  auto size = file->Size();
  if (!size.ok()) {
    return size.status();
  }
  if (*size == 0 || *size > 262144) {
    return absl::InvalidArgumentError("PEM file is empty or too large");
  }
  std::string pem(static_cast<std::size_t>(*size), '\0');
  std::size_t offset = 0;
  while (offset < pem.size()) {
    auto count = file->ReadAt(
        offset, std::as_writable_bytes(
                    std::span(pem.data(), pem.size()).subspan(offset)));
    if (!count.ok()) {
      return count.status();
    }
    if (*count == 0) {
      return absl::DataLossError("Truncated PEM file");
    }
    offset += *count;
  }
  return pem;
}
```

## Load an HTTPS page

Use the `LoadPem` helper above. This complete function fetches
`https://example.com/` using verified TLS 1.3 and HTTP/1.1. For TLS 1.2 change
only the selected version. For HTTP/2 change both ALPN and `Protocol` to `h2`
and `kHttp2`. The returned page is an explicit application buffer; process each
read chunk directly when you want a streamed download.

```cpp
#include <memory>
#include <string>

#include "absl/time/clock.h"
#include "symbian/api/connectivity/http.h"
#include "symbian/api/connectivity/tls_stream.h"

absl::StatusOr<std::string> LoadSecurePage() {
  namespace net = symbian::api::connectivity;
  auto roots = LoadPem(u"C:\\resource\\apps\\my_app_ca.pem");
  if (!roots.ok()) {
    return roots.status();
  }
  const auto deadline = absl::Now() + absl::Seconds(30);
  auto tcp = net::TcpClient::ConnectHost("example.com", 443, deadline);
  if (!tcp.ok()) {
    return tcp.status();
  }
  auto tls =
      net::TlsStream::Connect(std::move(*tcp), "example.com", *roots,
                              net::TlsVersion::kTls13, "http/1.1", deadline);
  if (!tls.ok()) {
    return tls.status();
  }
  symbian::http::RequestHead request;
  request.scheme = "https";
  request.authority = "example.com";
  request.headers = {{"user-agent", "MySymbianApp/1.0"}};
  auto exchange = symbian::http::Connection::Client(
      std::make_unique<net::TlsStream>(std::move(*tls)), std::move(request),
      symbian::http::Protocol::kHttp11, {}, 0, deadline);
  if (!exchange.ok()) {
    return exchange.status();
  }
  auto status = (*exchange)->Finish(deadline);
  if (!status.ok()) {
    return status;
  }
  status = (*exchange)->ReceiveHeaders(deadline);
  if (!status.ok()) {
    return status;
  }
  if ((*exchange)->response().status != 200) {
    return absl::UnavailableError("The page did not return HTTP 200");
  }
  std::string page;
  while (true) {
    auto chunk = (*exchange)->Read(deadline);
    if (!chunk.ok()) {
      return chunk.status();
    }
    if (!chunk->has_value()) {
      return page;
    }
    page.append(**chunk);
  }
}
```

An explicit-IP connection uses `TcpClient::ConnectIpv4` with the same verified
hostname passed to `TlsStream::Connect`; a routing address does not replace the
TLS identity. The request's `authority` sets the HTTP target. For a secure
WebSocket, pass this verified TLS transport to `WebSocketStream::Connect`
with ALPN `h2` and a peer that supports RFC 8441. The
[WebSocket guide](websocket.md) includes the masking source and full exchange.

## Serve HTTPS with client-certificate authentication

This complete function accepts one HTTPS request on loopback port 8443 and
sends a streamed response. Its parameters are PEM contents, not filenames;
load them using `LoadPem` if they are stored in application files. The caller
provisions its identity and selects the CA that may sign client certificates.
`Create` requires a client certificate, so ordinary browsers without one will
fail the handshake. `TlsServer` remains an alias for `TlsStream` for existing
callers. Select TLS 1.2/1.3 explicitly; the vendored server profile cannot
negotiate both versions in one configuration.

```cpp
#include <memory>
#include <string_view>

#include "absl/time/clock.h"
#include "symbian/api/connectivity/http.h"
#include "symbian/api/connectivity/tls_stream.h"

absl::Status ServeHttpsOnce(std::string_view certificate_pem,
                            std::string_view private_key_pem,
                            std::string_view client_ca_pem) {
  namespace net = symbian::api::connectivity;
  auto tls = net::TlsStream::Create(certificate_pem, private_key_pem,
                                    client_ca_pem, net::TlsVersion::kTls13);
  if (!tls.ok()) {
    return tls.status();
  }
  auto status = tls->SetAlpnProtocol("http/1.1");
  if (!status.ok()) {
    return status;
  }
  auto listener = net::TcpListener::ListenIpv4({127, 0, 0, 1}, 8443);
  if (!listener.ok()) {
    return listener.status();
  }
  const auto deadline = absl::Now() + absl::Seconds(30);
  auto tcp = listener->Accept(deadline);
  if (!tcp.ok()) {
    return tcp.status();
  }
  status = tls->Accept(std::move(*tcp), deadline);
  if (!status.ok()) {
    return status;
  }
  auto exchange = symbian::http::Connection::Accept(
      std::make_unique<net::TlsStream>(std::move(*tls)),
      symbian::http::Protocol::kHttp11, {}, deadline);
  if (!exchange.ok()) {
    return exchange.status();
  }
  while (true) {
    auto chunk = (*exchange)->Read(deadline);
    if (!chunk.ok()) {
      return chunk.status();
    }
    if (!chunk->has_value()) {
      break;
    }
  }
  status = (*exchange)->SendHeaders({200, {{"content-type", "text/plain"}}},
                                    deadline);
  if (!status.ok()) {
    return status;
  }
  status = (*exchange)->Write("Authenticated HTTPS response\n", deadline);
  return status.ok() ? (*exchange)->Finish(deadline) : status;
}
```

For an HTTP/2 server, call `SetAlpnProtocol("h2")` before `Accept` and give
`Connection::Accept` `Protocol::kHttp2`. Verify that
`negotiated_protocol()` is `h2` before dispatching HTTP/2. The connection closes
its transport on destruction or failure. `CloseSession` releases an inbound
session without a blocking TLS close alert and lets its configured state accept
another client. A client transport is likewise cancelled by closing its socket.

## Lower-level API

The original Mbed TLS C API remains available for advanced transport or identity
policy. Link `MbedTLS::mbedtls` after
`find_package(MbedTLS 3.4.1 EXACT CONFIG REQUIRED)`, or `MbedTLS::mbedcrypto` for
crypto alone. When combining the C package with Abseil-based native SDK APIs,
select the same streams runtime for their closure; the native `Symbian::Tls`
target already does so. The [TLS SDK reference](../reference/tls-sdk.md) describes
the packaged source, build targets and socket BIO requirements.
