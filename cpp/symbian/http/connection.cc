// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.
#include "symbian/http/connection.h"

#include <algorithm>
#include <array>

#include <absl/base/nullability.h>

namespace symbian::http {
Connection::Connection(net::ByteStream transport, Role role, Protocol protocol,
                       Limits limits)
    : transport_(std::move(transport)),
      role_(role),
      protocol_(protocol),
      limits_(limits) {}

Connection::~Connection() {
  transport_.Close();
}

absl::Status Connection::Fail(absl::Status status) {
  if (error_.ok()) {
    error_ = std::move(status);
  }
  transport_.Close();
  return error_;
}

void Connection::Abort() {
  Fail(absl::CancelledError("HTTP exchange aborted")).IgnoreError();
}

absl::Status Connection::Initialize() {
  if (!transport_ || limits_.maximum_buffered_bytes < 16384 ||
      limits_.maximum_buffered_bytes > 1048576 ||
      limits_.maximum_header_bytes == 0 ||
      limits_.maximum_header_bytes > 65536 || limits_.maximum_headers == 0 ||
      limits_.maximum_body_bytes == 0) {
    return absl::InvalidArgumentError(
        "Invalid HTTP connection limits or transport");
  }
  if (protocol_ == Protocol::kHttp2) {
    auto codec = Http2::Create(role_, limits_);
    if (!codec.ok()) {
      return codec.status();
    }
    h2_ = std::move(*codec);
  }
  return absl::OkStatus();
}

absl::Status Connection::Send(std::string_view bytes, absl::Time deadline) {
  if (!error_.ok()) {
    return error_;
  }
  while (!bytes.empty()) {
    auto size = std::min<std::size_t>(32768, bytes.size());
    if (auto status = transport_.Write(
            std::span(reinterpret_cast<const std::uint8_t*>(bytes.data()),
                      size),
            deadline);
        !status.ok()) {
      return Fail(status);
    }
    bytes.remove_prefix(size);
  }
  return absl::OkStatus();
}

absl::Status Connection::Flush(absl::Time deadline) {
  if (!error_.ok()) {
    return error_;
  }
  if (!h2_) {
    return absl::OkStatus();
  }
  auto output = h2_->TakeOutput();
  return output.ok() ? Send(*output, deadline) : Fail(output.status());
}

absl::Status Connection::Pump(absl::Time deadline) {
  auto status = Flush(deadline);
  if (!status.ok()) {
    return status;
  }
  std::array<std::uint8_t, 16384> bytes;
  auto count = transport_.Read(bytes, deadline);
  if (!count.ok()) {
    return Fail(count.status());
  }
  if (*count == 0) {
    eof_ = true;
    return h2_ ? Fail(absl::DataLossError(
                     "HTTP/2 transport EOF before END_STREAM"))
               : absl::OkStatus();
  }
  std::string_view data(reinterpret_cast<const char*>(bytes.data()), *count);
  if (h2_) {
    status = h2_->Feed(data);
    return status.ok() ? Flush(deadline) : Fail(status);
  }
  if (input_.size() + data.size() > limits_.maximum_buffered_bytes) {
    return Fail(
        absl::ResourceExhaustedError("HTTP receive buffer exceeds limit"));
  }
  input_.append(data);
  return absl::OkStatus();
}

absl::Status Connection::PrepareOutput(Headers* absl_nonnull headers,
                                       std::optional<std::size_t> length,
                                       bool no_body) {
  if (auto status = ValidateHeaders(*headers, limits_); !status.ok()) {
    return status;
  }
  for (const auto& name :
       {"content-length", "transfer-encoding", "connection", "host"}) {
    if (GetHeader(*headers, name)) {
      return absl::InvalidArgumentError(
          "HTTP framing/host fields are owned by the connection");
    }
  }
  if (length && *length > limits_.maximum_body_bytes) {
    return absl::ResourceExhaustedError("HTTP output body exceeds limit");
  }
  write_length_ = length;
  no_write_body_ = no_body;
  if (protocol_ == Protocol::kHttp11) {
    headers->emplace_back("connection", "close");
    if (length) {
      headers->emplace_back("content-length", std::to_string(*length));
    } else if (!no_body) {
      headers->emplace_back("transfer-encoding", "chunked");
    }
  } else if (length) {
    headers->emplace_back("content-length", std::to_string(*length));
  }
  return ValidateHeaders(*headers, limits_);
}

absl::StatusOr<std::unique_ptr<Connection>> Connection::Client(
    net::ByteStream transport, RequestHead request, Protocol protocol,
    Limits limits, std::optional<std::size_t> body_length,
    absl::Time deadline) {
  if (!transport) {
    return absl::InvalidArgumentError("Missing HTTP transport");
  }
  auto c = std::unique_ptr<Connection>(
      new Connection(std::move(transport), Role::kClient, protocol, limits));
  auto status = c->Initialize();
  if (!status.ok()) {
    return status;
  }
  status = ValidateRequest(request, limits);
  if (!status.ok()) {
    return status;
  }
  if (!request.protocol.empty() || request.method == "CONNECT") {
    return absl::InvalidArgumentError(
        "Use the duplex HTTP/2 primitive for CONNECT");
  }
  status = c->PrepareOutput(&request.headers, body_length, false);
  if (!status.ok()) {
    return status;
  }
  c->request_ = std::move(request);
  if (c->h2_) {
    status = c->h2_->SendRequest(c->request_);
  } else {
    c->request_.headers.emplace_back("host", c->request_.authority);
    status =
        c->Send(internal::SerializeRequest(c->request_.method, c->request_.path,
                                           c->request_.headers),
                deadline);
  }
  if (!status.ok()) {
    return status;
  }
  c->sent_head_ = true;
  status = c->Flush(deadline);
  if (!status.ok()) {
    return status;
  }
  return c;
}

absl::StatusOr<std::unique_ptr<Connection>> Connection::Accept(
    net::ByteStream transport, Protocol protocol, Limits limits,
    absl::Time deadline) {
  if (!transport) {
    return absl::InvalidArgumentError("Missing HTTP transport");
  }
  auto c = std::unique_ptr<Connection>(
      new Connection(std::move(transport), Role::kServer, protocol, limits));
  auto status = c->Initialize();
  if (!status.ok()) {
    return status;
  }
  status = c->ReadHead(deadline);
  if (!status.ok()) {
    return status;
  }
  return c;
}

absl::Status Connection::ReadHead(absl::Time deadline) {
  if (!error_.ok()) {
    return error_;
  }
  if (received_head_) {
    return absl::OkStatus();
  }
  unsigned informational = 0;
  while (!received_head_) {
    if (h2_) {
      if (h2_->headers_received()) {
        if (role_ == Role::kServer) {
          request_ = h2_->request();
        } else {
          response_ = h2_->response();
        }
        received_head_ = true;
        break;
      }
    } else if (auto end = internal::FindHeaderBlockEnd(input_)) {
      if (*end > limits_.maximum_header_bytes) {
        return Fail(absl::ResourceExhaustedError("HTTP head exceeds limit"));
      }
      if (role_ == Role::kServer) {
        auto head = internal::ParseRequestHead(
            std::string_view(input_).substr(0, *end));
        if (!head.ok()) {
          return Fail(head.status());
        }
        request_.method = head->method;
        request_.path = head->target;
        request_.headers = std::move(head->headers);
        request_.authority = GetHeader(request_.headers, "host").value_or("");
        auto status = ValidateRequest(request_, limits_);
        if (!status.ok()) {
          return Fail(status);
        }
        std::size_t hosts = 0;
        for (const auto& field : request_.headers) {
          if (field.first == "host") {
            ++hosts;
          }
        }
        if (hosts != 1) {
          return Fail(
              absl::InvalidArgumentError("HTTP requires one Host field"));
        }
        if (GetHeader(request_.headers, "expect")) {
          return Fail(absl::UnimplementedError("HTTP Expect is not supported"));
        }
        auto plan = internal::PlanRequestBody(request_.headers);
        if (!plan.ok()) {
          return Fail(plan.status());
        }
        plan_ = *plan;
      } else {
        auto head = internal::ParseResponseHead(
            std::string_view(input_).substr(0, *end));
        if (!head.ok()) {
          return Fail(head.status());
        }
        if (head->status < 200) {
          if (head->status == 101 || ++informational > 16) {
            return Fail(absl::InvalidArgumentError(
                "Unexpected HTTP informational response"));
          }
          input_.erase(0, *end);
          continue;
        }
        response_ = {.status = head->status,
                     .headers = std::move(head->headers)};
        if (auto valid = ValidateHeaders(response_.headers, limits_);
            !valid.ok()) {
          return Fail(valid);
        }
        auto plan = internal::PlanResponseBody(
            request_.method, response_.status, response_.headers);
        if (!plan.ok()) {
          return Fail(plan.status());
        }
        plan_ = *plan;
      }
      input_.erase(0, *end);
      if (plan_.content_length > limits_.maximum_body_bytes) {
        return Fail(absl::ResourceExhaustedError("HTTP body exceeds limit"));
      }
      received_head_ = true;
      break;
    } else if (input_.size() > limits_.maximum_header_bytes) {
      return Fail(absl::ResourceExhaustedError("HTTP head exceeds limit"));
    }
    if (eof_) {
      return Fail(absl::DataLossError("Truncated HTTP head"));
    }
    if (auto status = Pump(deadline); !status.ok()) {
      return status;
    }
  }
  return absl::OkStatus();
}

absl::Status Connection::ReceiveHeaders(absl::Time deadline) {
  return ReadHead(deadline);
}

absl::StatusOr<std::optional<std::string>> Connection::Read(
    absl::Time deadline) {
  auto status = ReadHead(deadline);
  if (!status.ok()) {
    return status;
  }
  while (!read_end_) {
    std::string output;
    if (h2_) {
      auto chunk = h2_->Read();
      if (!chunk.ok()) {
        return Fail(chunk.status());
      }
      if (chunk->has_value()) {
        output = std::move(**chunk);
      } else if (h2_->ended()) {
        read_end_ = true;
        trailers_ = h2_->trailers();
      }
      status = Flush(deadline);
      if (!status.ok()) {
        return status;
      }
    } else {
      switch (plan_.framing) {
        case internal::BodyFraming::kNone:
          read_end_ = true;
          break;
        case internal::BodyFraming::kContentLength: {
          auto count =
              std::min(input_.size(), plan_.content_length - read_bytes_);
          output = input_.substr(0, count);
          input_.erase(0, count);
          read_end_ = read_bytes_ + count == plan_.content_length;
          break;
        }
        case internal::BodyFraming::kChunked:
          status = decoder_.Feed(input_, &output, &read_end_);
          input_.clear();
          if (!status.ok()) {
            return Fail(status);
          }
          if (read_end_) {
            trailers_ = decoder_.trailers();
          }
          break;
        case internal::BodyFraming::kUntilClose:
          output.swap(input_);
          read_end_ = eof_;
          break;
      }
    }
    if (!output.empty()) {
      if (output.size() > limits_.maximum_body_bytes - read_bytes_) {
        return Fail(absl::ResourceExhaustedError("HTTP body exceeds limit"));
      }
      read_bytes_ += output.size();
      return std::optional<std::string>(std::move(output));
    }
    if (read_end_) {
      break;
    }
    if (eof_) {
      return Fail(absl::DataLossError("Truncated HTTP body"));
    }
    status = Pump(deadline);
    if (!status.ok()) {
      return status;
    }
  }
  return std::optional<std::string>();
}

absl::Status Connection::SendHeaders(ResponseHead head, absl::Time deadline,
                                     std::optional<std::size_t> body_length) {
  if (!error_.ok()) {
    return error_;
  }
  if (role_ != Role::kServer || sent_head_ || !received_head_) {
    return absl::FailedPreconditionError("HTTP response out of order");
  }
  if (head.status < 200 || head.status > 599) {
    return absl::InvalidArgumentError("Invalid HTTP response status");
  }
  bool no_body =
      request_.method == "HEAD" || head.status == 204 || head.status == 304;
  if (head.status == 204) {
    body_length = std::nullopt;
  }
  auto status = PrepareOutput(&head.headers, body_length, no_body);
  if (!status.ok()) {
    return status;
  }
  response_ = std::move(head);
  status = h2_ ? h2_->SendHeaders(response_)
               : Send(internal::SerializeResponse(response_.status,
                                                  response_.headers),
                      deadline);
  if (!status.ok()) {
    return Fail(status);
  }
  sent_head_ = true;
  return Flush(deadline);
}

absl::Status Connection::Write(std::string_view bytes, absl::Time deadline) {
  if (!error_.ok()) {
    return error_;
  }
  if (!sent_head_ || write_end_) {
    return absl::FailedPreconditionError("HTTP write out of order");
  }
  if (bytes.size() > 32768) {
    return absl::ResourceExhaustedError("HTTP write exceeds 32 KiB");
  }
  if ((no_write_body_ && !bytes.empty()) ||
      bytes.size() > limits_.maximum_body_bytes - written_bytes_ ||
      (write_length_ && bytes.size() > *write_length_ - written_bytes_)) {
    return absl::InvalidArgumentError(
        "HTTP write exceeds declared body length");
  }
  auto status = h2_ ? h2_->Write(bytes)
                    : Send(write_length_ ? std::string(bytes)
                                         : internal::EncodeChunk(bytes),
                           deadline);
  if (!status.ok()) {
    return Fail(status);
  }
  written_bytes_ += bytes.size();
  if (h2_) {
    do {
      status = Flush(deadline);
      if (!status.ok()) {
        return status;
      }
      if (h2_->buffered_amount()) {
        status = Pump(deadline);
        if (!status.ok()) {
          return status;
        }
      }
    } while (h2_->buffered_amount());
  }
  return absl::OkStatus();
}

absl::Status Connection::Finish(absl::Time deadline) {
  if (!error_.ok()) {
    return error_;
  }
  if (!sent_head_) {
    return absl::FailedPreconditionError("HTTP finish before headers");
  }
  if (write_end_) {
    return absl::OkStatus();
  }
  if (!no_write_body_ && write_length_ && written_bytes_ != *write_length_) {
    return Fail(absl::DataLossError("HTTP body shorter than declared length"));
  }
  if (auto status = h2_ ? h2_->Finish()
                        : (!write_length_ && !no_write_body_
                               ? Send(internal::EncodeLastChunk(), deadline)
                               : absl::OkStatus());
      !status.ok()) {
    return Fail(status);
  }
  write_end_ = true;
  return Flush(deadline);
}
}  // namespace symbian::http
