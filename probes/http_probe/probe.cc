// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.
#include "absl/time/clock.h"
#include "probe_config.h"
#include "symbian/api/connectivity/http.h"
#include "symbian/api/connectivity/tls_stream.h"
#include "symbian/api/storage/storage.h"

namespace {
using namespace symbian::api;

absl::Status Save(std::string_view report) {
  auto file = storage::WritableFile::Open(u"C:\\http-result.txt",
                                          storage::WriteMode::kReplaceExisting);
  if (!file.ok()) {
    return file.status();
  }
  auto status =
      file->WriteAt(0, std::as_bytes(std::span(report.data(), report.size())));
  return status.ok() ? file->Flush() : status;
}

int RunServer(bool http2, int tls_version = 0) {
  using namespace connectivity;
  auto deadline = absl::Now() + absl::Seconds(40);
  const auto protocol = http2 ? symbian::http::Protocol::kHttp2
                              : symbian::http::Protocol::kHttp11;
  absl::StatusOr<std::unique_ptr<symbian::http::Connection>> connection =
      absl::InternalError("No server connection");
  if (tls_version) {
    auto listener = TcpListener::ListenIpv4({127, 0, 0, 1}, 39105);
    if (!listener.ok()) {
      return -320;
    }
    auto tls = TlsStream::Create(
        kServerCertificate, kServerKey, kServerCertificate,
        tls_version == 12 ? TlsVersion::kTls12 : TlsVersion::kTls13);
    if (!tls.ok()) {
      Save(tls.status().ToString()).IgnoreError();
      return -326;
    }
    auto status = tls->SetAlpnProtocol(http2 ? "h2" : "http/1.1");
    if (!status.ok()) {
      return -327;
    }
    auto tcp = listener->Accept(deadline);
    if (!tcp.ok()) {
      return -328;
    }
    status = tls->Accept(std::move(*tcp), deadline);
    if (!status.ok()) {
      Save(status.ToString()).IgnoreError();
      return -329;
    }
    connection = symbian::http::Connection::Accept(
        std::make_unique<TlsStream>(std::move(*tls)), protocol, {}, deadline);
  } else {
    auto server = HttpServer::ListenIpv4({127, 0, 0, 1}, 39105, protocol);
    if (!server.ok()) {
      Save(server.status().ToString()).IgnoreError();
      return -320;
    }
    connection = server->Accept(deadline);
  }
  if (!connection.ok()) {
    Save(connection.status().ToString()).IgnoreError();
    return -321;
  }
  if ((*connection)->request().method != "POST" ||
      (*connection)->request().path != "/upload") {
    return -322;
  }
  std::size_t bytes = 0;
  while (true) {
    auto chunk = (*connection)->Read(deadline);
    if (!chunk.ok()) {
      Save(chunk.status().ToString()).IgnoreError();
      return -323;
    }
    if (!chunk->has_value()) {
      break;
    }
    bytes += (**chunk).size();
  }
  auto status =
      (*connection)
          ->SendHeaders({200, {{"content-type", "text/plain"}}}, deadline);
  if (!status.ok()) {
    Save(status.ToString()).IgnoreError();
    return -324;
  }
  status = (*connection)->Write("native HTTP stream\n", deadline);
  if (status.ok()) {
    status = (*connection)->Write(std::to_string(bytes) + "\n", deadline);
  }
  if (status.ok()) {
    status = (*connection)->Finish(deadline);
  }
  Save(status.ToString()).IgnoreError();
  return status.ok() ? 0 : -325;
}

int Run(const ProbeCase& test) {
  using namespace connectivity;
  auto deadline = absl::Now() + absl::Seconds(40);
  auto tcp = TcpClient::ConnectHost(test.hostname, test.port, deadline);
  if (!tcp.ok()) {
    Save(tcp.status().ToString()).IgnoreError();
    return -301;
  }
  symbian::net::ByteStream stream;
  std::string tls_version, alpn;
  if (test.tls_version) {
    auto tls = TlsStream::Connect(
        std::move(*tcp),
        test.reject_certificate ? "wrong.invalid" : test.hostname, kRoots,
        test.tls_version == 12 ? TlsVersion::kTls12 : TlsVersion::kTls13,
        test.http2 ? "h2" : "http/1.1", deadline);
    if (!tls.ok()) {
      Save(tls.status().ToString()).IgnoreError();
      return test.reject_certificate ? 0 : -302;
    }
    if (test.reject_certificate) {
      Save("unexpected certificate acceptance").IgnoreError();
      return -303;
    }
    tls_version = tls->negotiated_version();
    alpn = tls->negotiated_protocol();
    stream = std::make_unique<TlsStream>(std::move(*tls));
  } else {
    stream = std::make_unique<TcpByteStream>(std::move(*tcp));
  }
  symbian::http::RequestHead request;
  request.authority = test.hostname;
  request.scheme = test.tls_version ? "https" : "http";
  request.headers = {{"user-agent", "Symbian-HTTP-acceptance/1.0"},
                     {"accept-encoding", "identity"}};
  auto connection = symbian::http::Connection::Client(
      std::move(stream), request,
      test.http2 ? symbian::http::Protocol::kHttp2
                 : symbian::http::Protocol::kHttp11,
      {}, 0, deadline);
  if (!connection.ok()) {
    Save(connection.status().ToString()).IgnoreError();
    return -304;
  }
  auto status = (*connection)->Finish(deadline);
  if (!status.ok()) {
    Save(status.ToString()).IgnoreError();
    return -305;
  }
  status = (*connection)->ReceiveHeaders(deadline);
  if (!status.ok()) {
    Save(status.ToString()).IgnoreError();
    return -306;
  }
  std::size_t bytes = 0, chunks = 0;
  std::string prefix;
  while (true) {
    auto chunk = (*connection)->Read(deadline);
    if (!chunk.ok()) {
      Save(chunk.status().ToString()).IgnoreError();
      return -307;
    }
    if (!chunk->has_value()) {
      break;
    }
    bytes += (**chunk).size();
    ++chunks;
    if (prefix.size() < 200) {
      prefix.append((**chunk).substr(0, 200 - prefix.size()));
    }
  }
  const std::string report =
      "host=" + std::string(test.hostname) + "\ntls=" + tls_version +
      "\nalpn=" + alpn +
      "\nstatus=" + std::to_string((*connection)->response().status) +
      "\nbytes=" + std::to_string(bytes) +
      "\nchunks=" + std::to_string(chunks) + "\nprefix=" + prefix + "\n";
  if (!Save(report).ok()) {
    return -308;
  }
  return bytes > 0 && (*connection)->response().status >= 200 &&
                 (*connection)->response().status < 400
             ? 0
             : -309;
}
}  // namespace

int main() {
  auto file = storage::ReadOnlyFile::Open(u"C:\\http-case.txt");
  if (!file.ok()) {
    return -310;
  }
  std::array<std::byte, 1> input{};
  auto read = file->ReadAt(0, input);
  if (!read.ok() || *read != 1) {
    return -311;
  }
  if (input[0] == std::byte{'u'}) {
    return RunServer(false, 12);
  }
  if (input[0] == std::byte{'v'}) {
    return RunServer(true, 13);
  }
  if (input[0] == std::byte{'s'}) {
    return RunServer(false);
  }
  if (input[0] == std::byte{'t'}) {
    return RunServer(true);
  }
  unsigned index = std::to_integer<unsigned>(input[0]) - '0';
  if (index >= std::size(kCases)) {
    return -312;
  }
  return Run(kCases[index]);
}
