// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "python/agent_bindings.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "python/status_interop.h"
#include "symbian/agent/control.h"
#include "symbian/agent/frame.h"

namespace symbian::python {
namespace {

namespace py = pybind11;

py::bytes PackAgentReadRequest(std::uint64_t request_id, int kind,
                               std::uint64_t deadline_millis) {
  auto frame = ValueWithoutGil([&]() -> absl::StatusOr<std::string> {
    if (request_id == 0 || (kind != 1 && kind != 2)) {
      return absl::InvalidArgumentError("Expected a nonzero hello/status ID");
    }
    agent::ControlMessage message;
    message.request_id = request_id;
    message.kind = static_cast<agent::ControlKind>(kind);
    message.deadline_millis = deadline_millis;
    auto payload = agent::PackControl(message);
    if (!payload.ok()) {
      return payload.status();
    }
    return agent::EncodeFrame(
        std::span(reinterpret_cast<const std::uint8_t*>(payload->data()),
                  payload->size()),
        agent::kMaximumControlBytes);
  });
  return py::bytes(frame);
}

py::bytes PackAgentLogsRequest(std::uint64_t request_id, std::uint64_t after,
                               int limit) {
  auto frame = ValueWithoutGil([&]() -> absl::StatusOr<std::string> {
    if (request_id == 0 || limit < 1 || limit > 8) {
      return absl::InvalidArgumentError("Invalid agent log cursor or limit");
    }
    agent::ControlMessage message;
    message.request_id = request_id;
    message.kind = agent::ControlKind::kLogs;
    message.body = {{"after", after}, {"limit", limit}};
    auto payload = agent::PackControl(message);
    if (!payload.ok()) {
      return payload.status();
    }
    return agent::EncodeFrame(
        std::span(reinterpret_cast<const std::uint8_t*>(payload->data()),
                  payload->size()),
        agent::kMaximumControlBytes);
  });
  return py::bytes(frame);
}

py::bytes PackAgentWorkspaceRequest(std::uint64_t request_id,
                                    std::uint64_t after, int limit) {
  auto frame = ValueWithoutGil([&]() -> absl::StatusOr<std::string> {
    if (request_id == 0 || after > 256 || limit < 1 || limit > 8) {
      return absl::InvalidArgumentError("Invalid workspace page");
    }
    agent::ControlMessage message;
    message.request_id = request_id;
    message.kind = agent::ControlKind::kWorkspaceList;
    message.body = {{"after", after}, {"limit", limit}};
    auto payload = agent::PackControl(message);
    if (!payload.ok()) {
      return payload.status();
    }
    return agent::EncodeFrame(
        std::span(reinterpret_cast<const std::uint8_t*>(payload->data()),
                  payload->size()),
        agent::kMaximumControlBytes);
  });
  return py::bytes(frame);
}

py::dict ParseAgentResultFrame(const py::bytes& input) {
  const std::string bytes = input;
  auto result = ValueWithoutGil([&]() -> absl::StatusOr<agent::ControlMessage> {
    if (bytes.size() > agent::kMaximumControlBytes + 4) {
      return absl::ResourceExhaustedError("Agent result exceeds 4 KiB");
    }
    agent::FrameDecoder decoder(agent::kMaximumControlBytes);
    std::optional<agent::Frame> frame;
    auto consumed = decoder.Consume(
        std::span(reinterpret_cast<const std::uint8_t*>(bytes.data()),
                  bytes.size()),
        &frame);
    if (!consumed.ok()) {
      return consumed.status();
    }
    if (!frame || *consumed != bytes.size()) {
      return absl::InvalidArgumentError("Incomplete or trailing agent frame");
    }
    auto message = agent::ParseControl(frame->payload);
    if (!message.ok()) {
      return message.status();
    }
    if (message->kind != agent::ControlKind::kResult &&
        message->kind != agent::ControlKind::kError) {
      return absl::InvalidArgumentError("Expected an agent result or error");
    }
    return *message;
  });
  py::dict parsed;
  parsed["request_id"] = result.request_id;
  parsed["kind"] = static_cast<int>(result.kind);
  parsed["deadline_millis"] = result.deadline_millis;
  auto json = py::module_::import("json");
  parsed["body"] = json.attr("loads")(result.body.dump());
  parsed["extensions"] = json.attr("loads")(result.extensions.dump());
  return parsed;
}

std::size_t AgentControlPayloadLength(const py::bytes& input) {
  const std::string prefix = input;
  return ValueWithoutGil([&] {
    return agent::DecodeFrameLength(
        std::span(reinterpret_cast<const std::uint8_t*>(prefix.data()),
                  prefix.size()),
        agent::kMaximumControlBytes);
  });
}

}  // namespace

void BindAgent(py::module_& module) {
  module.def("pack_agent_read_request", &PackAgentReadRequest,
             py::arg("request_id"), py::arg("kind"),
             py::arg("deadline_millis") = 0,
             "Pack a bounded hello/status frame with native MessagePack.");
  module.def("pack_agent_logs_request", &PackAgentLogsRequest,
             py::arg("request_id"), py::arg("after"), py::arg("limit") = 8,
             "Pack a bounded, read-only agent log cursor request.");
  module.def("pack_agent_workspace_request", &PackAgentWorkspaceRequest,
             py::arg("request_id"), py::arg("after") = 0, py::arg("limit") = 8,
             "Pack a bounded, read-only agent workspace listing request.");
  module.def(
      "parse_agent_result_frame", &ParseAgentResultFrame, py::arg("frame"),
      "Parse one complete bounded result frame with native MessagePack.");
  module.def(
      "agent_control_payload_length", &AgentControlPayloadLength,
      py::arg("prefix"),
      "Validate the four-byte control prefix before payload allocation.");
}

}  // namespace symbian::python
