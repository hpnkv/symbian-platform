# Native HTTP client and server

Link `Symbian::Http` in a generated SDK application:

```cmake
target_link_libraries(my_app PRIVATE Symbian::Http)
```

Include `symbian/api/connectivity/http.h`. The client and listener use native
`RSocket`; `TcpClient::ConnectHost` resolves an ASCII hostname with the native
resolver. Operations use one absolute deadline, cancel and drain expired native
requests, and return `absl::Status` or `StatusOr`. Run these blocking functions
on an SDK worker, outside UI/active-scheduler callbacks. They create no threads
or scheduler themselves. Declare `NetworkServices` in `symbian.toml`.

## Load a page and set request headers

This complete function fetches `http://example.com/`, checks the response status,
and returns its body. It includes the request completion and body-read loop.
The result is deliberately buffered by the application; streaming callers can
process each `chunk` immediately instead of appending it.

```cpp
#include <string>
#include "absl/time/clock.h"
#include "symbian/api/connectivity/http.h"

absl::StatusOr<std::string> LoadPage() {
  namespace net = symbian::api::connectivity;
  const auto deadline = absl::Now() + absl::Seconds(20);
  symbian::http::RequestHead request;
  request.authority = "example.com";
  request.path = "/";
  request.headers = {{"user-agent", "MySymbianApp/1.0"},
                     {"accept", "text/html"}};
  auto exchange = net::HttpClient::ConnectHost(
      "example.com", 80, std::move(request), deadline);
  if (!exchange.ok()) return exchange.status();
  auto status = (*exchange)->Finish(deadline);
  if (!status.ok()) return status;
  status = (*exchange)->ReceiveHeaders(deadline);
  if (!status.ok()) return status;
  if ((*exchange)->response().status != 200)
    return absl::UnavailableError("The page did not return HTTP 200");
  std::string page;
  while (true) {
    auto chunk = (*exchange)->Read(deadline);
    if (!chunk.ok()) return chunk.status();
    if (!chunk->has_value()) return page;
    page.append(**chunk);
  }
}
```

Headers are an ordered sequence of name/value pairs, so repeated fields retain
wire order. Add another application header with
`request.headers.emplace_back("x-request-id", "123")`. Inspect an incoming field
with `symbian::http::GetHeader(exchange->response().headers, "content-type")`
when `exchange` is a `Connection`. HTTP/2 names are normalized to lowercase.
The connection owns `Host`, `Content-Length`, `Transfer-Encoding` and
`Connection`; set `authority` and the body-length argument instead of supplying
those fields. Values containing control characters are rejected.

For an upload, set `request.method = "POST"` and pass the known size as the
`body_length` argument to `ConnectHost`, then call `Write` for each chunk before
`Finish`. Pass `std::nullopt` for an unknown length: HTTP/1.1 uses chunked
transfer encoding, and HTTP/2 uses DATA followed by END_STREAM. Each `Write`
accepts at most 32 KiB. An early `Finish` on a fixed-length body is an error.

## Serve one request with a streamed response

Run this function on a worker. It listens on loopback port 8080, reads the
request body incrementally, then sends the response in two writes. Call it
again, or keep a listener and accept successive exchanges, to serve more
requests. Accepted connections own their sockets independently of the listener.

```cpp
#include "absl/time/clock.h"
#include "symbian/api/connectivity/http.h"

absl::Status ServeOnce() {
  namespace net = symbian::api::connectivity;
  auto server = net::HttpServer::ListenIpv4({127, 0, 0, 1}, 8080);
  if (!server.ok()) return server.status();
  const auto deadline = absl::Now() + absl::Seconds(30);
  auto exchange = server->Accept(deadline);
  if (!exchange.ok()) return exchange.status();
  while (true) {
    auto chunk = (*exchange)->Read(deadline);
    if (!chunk.ok()) return chunk.status();
    if (!chunk->has_value()) break;
    // Process this request-body chunk here.
  }
  auto status = (*exchange)->SendHeaders(
      {200, {{"content-type", "text/plain; charset=utf-8"}}}, deadline);
  if (!status.ok()) return status;
  status = (*exchange)->Write("Hello ", deadline);
  if (!status.ok()) return status;
  status = (*exchange)->Write("from Symbian!\n", deadline);
  if (!status.ok()) return status;
  return (*exchange)->Finish(deadline);
}
```

`Accept` returns after the request head arrives. Inspect
`(*exchange)->request().method`, `.path` and `.headers` to route it. Default
response framing supports an unknown length. Supply a third `SendHeaders`
argument for a fixed length; the connection verifies the number of bytes sent.
HEAD, 204 and 304 responses suppress body writes. Trailers are available through
`trailers()` after `Read` reports the end. `Abort()` closes the owned transport.

## Choose HTTP/2 or TLS

Pass `symbian::http::Protocol::kHttp2` to the client or server for cleartext
prior-knowledge HTTP/2. An ordinary HTTP/1.1 client cannot talk to that listener.
For HTTPS, establish `TlsStream::Connect`, then give the verified transport to
`Connection::Client`; the [TLS guide](tls.md) includes complete client and
server examples. The [WebSocket guide](websocket.md) shows the same transport
and shared HTTP/2 primitives carrying RFC 8441 duplex WebSockets.

## Streaming contract and bounds

`Read` returns a nonempty string chunk, `nullopt` at a clean end, or an error.
Chunk boundaries are transport boundaries, not application messages. Do not
interpret a timeout, reset, truncated Content-Length or incomplete chunked body
as success. TLS close-delimited bodies require close_notify; an unannounced
TLS transport EOF is an error. Fixed-length and HTTP/2 bodies finish from their
own framing without waiting for transport closure.

| Default limit | Value |
| --- | --- |
| Header block / number of fields | 16 KiB / 64 |
| Buffered inbound/outbound body bytes | 64 KiB per direction |
| Total request or response body | 32 MiB |
| One application write | 32 KiB |

Pass `symbian::http::Limits` to change the bounds. HTTP/2 returns flow-control
credit when the application reads DATA; a paused consumer bounds buffering and
stalls the sender. HTTP/1.1 reads only when the application asks. The request
read half and response writer are separate, following A11's body-stream model.
The synchronous facade handles one exchange per connection. Connection pooling,
redirects, decompression, server push, HTTP/1 Upgrade and multiplexed exchanges
are not implemented. `Expect` requests are rejected explicitly. Extended
CONNECT uses the shared `Http2` duplex primitive below the WebSocket facade.

The emulator acceptance suite exercises public HTTP/HTTPS pages, forces and
records TLS 1.2/1.3 and ALPN, rejects wrong hostnames, and tests native servers
against Python's HTTP/1.1 and independent hyper-h2 codecs. Replay:

```sh
SYMBIAN_HTTP_LIVE_GUEST=1 \
SYMBIAN_SDK_MANIFEST=/absolute/sdk/sdk.json \
python -m pytest symbian/tests/test_http_guest.py -v
```

By default this test builds the current workspace libraries against the selected
SDK dependencies. Set `SYMBIAN_HTTP_EXPORTED_SDK=1` to use only exported SDK
libraries. The opt-in RM-807 entropy adapter is a named emulator experiment;
these results establish neither general firmware nor physical-device support.
Exact inputs, outcomes and retained failures are in `.dev/research-log.md`.
