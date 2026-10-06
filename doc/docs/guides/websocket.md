# Native WebSocket connections

The native client/server implement RFC 6455 binary messages over RFC 8441
HTTP/2 extended CONNECT. Both peers must support that protocol: this does not
implement browser-style HTTP/1.1 Upgrade or connect to an arbitrary public
`wss://` echo service. HTTP and WebSockets share the SDK's TCP/TLS transport and
bounded nghttp2 DATA layer.

Link the native stream wrapper and crypto library in a generated application:

```cmake
target_link_libraries(my_app PRIVATE Symbian::WebSocket Symbian::Crypto)
```

## Connect, send, receive and close

This complete function connects to a compatible listener at
`localhost:39106/echo`, sends `hello`, verifies the five-byte reply and performs
the close handshake. The Mbed TLS entropy interface uses the SDK's verified
OS secure RNG provider, just as TLS does. There is no fixed or
weak-random masking fallback. Its state remains alive for the entire connection.

```cpp
#include <array>
#include <span>

#include "absl/time/clock.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "symbian/api/connectivity/websocket.h"

absl::Status WebSocketRoundTrip() {
  namespace net = symbian::api::connectivity;

  struct Random {
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context rng;

    Random() {
      mbedtls_entropy_init(&entropy);
      mbedtls_ctr_drbg_init(&rng);
    }

    ~Random() {
      mbedtls_ctr_drbg_free(&rng);
      mbedtls_entropy_free(&entropy);
    }
  } random;

  if (mbedtls_ctr_drbg_seed(&random.rng, mbedtls_entropy_func, &random.entropy,
                            nullptr, 0) != 0) {
    return absl::UnavailableError("Cryptographic entropy unavailable");
  }
  const auto deadline = absl::Now() + absl::Seconds(20);
  auto tcp = net::TcpClient::ConnectHost("localhost", 39106, deadline);
  if (!tcp.ok()) {
    return tcp.status();
  }
  symbian::websocket::Options options;
  options.path = "/echo";
  options.authority = "localhost:39106";
  options.mask_provider =
      [&random]() -> absl::StatusOr<std::array<std::uint8_t, 4>> {
    std::array<std::uint8_t, 4> mask;
    if (mbedtls_ctr_drbg_random(&random.rng, mask.data(), mask.size()) != 0) {
      return absl::UnavailableError("WebSocket masking entropy failed");
    }
    return mask;
  };
  auto stream = net::WebSocketStream::Connect(std::move(*tcp),
                                              std::move(options), deadline);
  if (!stream.ok()) {
    return stream.status();
  }
  const std::array<std::uint8_t, 5> sent{'h', 'e', 'l', 'l', 'o'};
  auto status = stream->Send(sent, deadline);
  if (!status.ok()) {
    return status;
  }
  std::array<std::uint8_t, 5> received;
  std::size_t offset = 0;
  while (offset < received.size()) {
    auto count = stream->Receive(std::span(received).subspan(offset), deadline);
    if (!count.ok()) {
      return count.status();
    }
    if (*count == 0) {
      return absl::DataLossError("Incomplete echo");
    }
    offset += *count;
  }
  if (received != sent) {
    return absl::DataLossError("Wrong echo");
  }
  return stream->Close(deadline);
}
```

The matching native echo server is:

```cpp
#include <array>
#include <span>

#include "absl/time/clock.h"
#include "symbian/api/connectivity/websocket.h"

absl::Status WebSocketEchoOnce() {
  namespace net = symbian::api::connectivity;
  symbian::websocket::Options options;
  options.path = "/echo";
  auto server = net::WebSocketServer::ListenIpv4({127, 0, 0, 1}, 39106,
                                                 std::move(options));
  if (!server.ok()) {
    return server.status();
  }
  const auto deadline = absl::Now() + absl::Seconds(30);
  auto stream = server->Accept(deadline);
  if (!stream.ok()) {
    return stream.status();
  }
  std::array<std::uint8_t, 5> message;
  std::size_t offset = 0;
  while (offset < message.size()) {
    auto count = stream->Receive(std::span(message).subspan(offset), deadline);
    if (!count.ok()) {
      return count.status();
    }
    if (*count == 0) {
      return absl::DataLossError("Incomplete request");
    }
    offset += *count;
  }
  auto status = stream->Send(message, deadline);
  return status.ok() ? stream->Close(deadline) : status;
}
```

## TLS and lifetime

For a secure connection, use `TlsStream::Connect` with ALPN `h2`, then pass
`std::make_unique<TlsStream>(std::move(*tls))` to the transport overload of
`WebSocketStream::Connect`. Use the same path and authority options. The
[TLS guide](tls.md) supplies the full certificate/hostname setup. The server can
likewise pass a verified accepted TLS transport to `WebSocketStream::Accept`.
The peer must advertise RFC 8441 support over the negotiated HTTP/2 connection.

Keep calls, transport destruction and the masking source on one owning SDK
worker. All operations take absolute deadlines. `Close` sends the close frame
and waits for its peer reply; `Abort` closes immediately. `WebSocketStream`
exposes a byte stream across binary messages for application framing. The
I/O-free `symbian::websocket::WebSocket` codec instead exposes complete messages
through `Send`/`Receive`, and `Feed`/`TakeOutput` let a caller drive its transport.
Default message, queue and header limits remain explicit in `Options` and the
native API. No Python callback, new scheduler or connection authentication
policy is supplied by the WebSocket library.
