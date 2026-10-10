// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "python/agent_bindings.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>

#include <absl/base/nullability.h>

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

py::bytes PackAgentDisplayRequest(std::uint64_t request_id, int kind,
                                  int x, int y, int action) {
  auto frame = ValueWithoutGil([&]() -> absl::StatusOr<std::string> {
    if (request_id == 0 || (kind != 8 && kind != 9) ||
        (kind == 9 && (x < 0 || y < 0 || x > 4095 || y > 4095 ||
                       action < 1 || action > 3))) {
      return absl::InvalidArgumentError("Invalid display request");
    }
    agent::ControlMessage message;
    message.request_id = request_id;
    message.kind = static_cast<agent::ControlKind>(kind);
    if (kind == 9) {
      message.body = {{"x", x}, {"y", y}, {"action", action}};
    }
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

py::bytes PackAgentResourceRequest(std::uint64_t request_id, int kind,
                                   int scope, std::uint32_t uid,
                                   const std::string& name,
                                   std::uint64_t offset, int length,
                                   int mode) {
  auto frame = ValueWithoutGil([&]() -> absl::StatusOr<std::string> {
    if (request_id == 0 || (kind != 10 && kind != 11) || scope < 0 ||
        scope > 1 || (scope == 0 && uid != 0) ||
        (scope == 1 && uid == 0) || name.empty() || name.size() > 64 ||
        name == "." || name == ".." || offset > 16 * 1024 * 1024 ||
        length < 1 || length > 32768 || mode < 0 || mode > 2) {
      return absl::InvalidArgumentError("Invalid resource request");
    }
    for (char ch : name) {
      if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') || ch == '.' || ch == '_' || ch == '-')) {
        return absl::InvalidArgumentError("Invalid resource name");
      }
    }
    agent::ControlMessage message;
    message.request_id = request_id;
    message.kind = static_cast<agent::ControlKind>(kind);
    message.body = {{"scope", scope}, {"uid", uid}, {"name", name},
                    {"offset", offset}, {"length", length}};
    if (kind == 11) {
      message.body["mode"] = mode;
    }
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

py::bytes PackAgentApplicationRequest(std::uint64_t request_id, int kind,
                                      std::uint32_t uid,
                                      const std::string& name) {
  auto frame = ValueWithoutGil([&]() -> absl::StatusOr<std::string> {
    if (request_id == 0 || (kind != 12 && kind != 13) || uid == 0 ||
        (kind == 12 &&
         (name.size() < 5 || name.size() > 64 ||
          name.substr(name.size() - 4) != ".sis"))) {
      return absl::InvalidArgumentError("Invalid application request");
    }
    if (kind == 12) {
      for (char ch : name) {
        if (!((ch >= 'a' && ch <= 'z') ||
              (ch >= 'A' && ch <= 'Z') ||
              (ch >= '0' && ch <= '9') || ch == '.' || ch == '_' ||
              ch == '-')) {
          return absl::InvalidArgumentError("Invalid package name");
        }
      }
    }
    agent::ControlMessage message;
    message.request_id = request_id;
    message.kind = static_cast<agent::ControlKind>(kind);
    message.body = {{"uid", uid}};
    if (kind == 12) {
      message.body["name"] = name;
    }
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

void BindAgent(py::module_* absl_nonnull module) {
  BindWebSocket(module);
  module->def("pack_agent_read_request", &PackAgentReadRequest,
              py::arg("request_id"), py::arg("kind"),
              py::arg("deadline_millis") = 0,
              "Pack a bounded hello/status frame with native MessagePack.");
  module->def("pack_agent_logs_request", &PackAgentLogsRequest,
              py::arg("request_id"), py::arg("after"), py::arg("limit") = 8,
              "Pack a bounded, read-only agent log cursor request.");
  module->def("pack_agent_workspace_request", &PackAgentWorkspaceRequest,
              py::arg("request_id"), py::arg("after") = 0, py::arg("limit") = 8,
              "Pack a bounded, read-only agent workspace listing request.");
  module->def("pack_agent_display_request", &PackAgentDisplayRequest,
              py::arg("request_id"), py::arg("kind"), py::arg("x") = 0,
              py::arg("y") = 0, py::arg("action") = 0,
              "Pack a screen capture or pointer input request.");
  module->def("pack_agent_resource_request", &PackAgentResourceRequest,
              py::arg("request_id"), py::arg("kind"), py::arg("scope"),
              py::arg("uid"), py::arg("name"), py::arg("offset"),
              py::arg("length"), py::arg("mode") = 0,
              "Pack one bounded app-shared or agent resource request.");
  module->def("pack_agent_application_request", &PackAgentApplicationRequest,
              py::arg("request_id"), py::arg("kind"), py::arg("uid"),
              py::arg("name") = "",
              "Pack an installer launch or AppArc registration query.");
  module->def(
      "parse_agent_result_frame", &ParseAgentResultFrame, py::arg("frame"),
      "Parse one complete bounded result frame with native MessagePack.");
  module->def(
      "agent_control_payload_length", &AgentControlPayloadLength,
      py::arg("prefix"),
      "Validate the four-byte control prefix before payload allocation.");
}

}  // namespace symbian::python
