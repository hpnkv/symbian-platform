// Copyright 2026 The A11 Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cstdint>
#include <exception>
#include <string>

#include <absl/base/nullability.h>
#include <nlohmann/json.hpp>
#include <pybind11/operators.h>
#include <pybind11_abseil/status_casters.h>

#include "python/status_interop.h"
#include "symbian/status/status.h"

namespace symbian::python {
namespace {
absl::StatusCode CanonicalStatusCode(int value) {
  if (value < static_cast<int>(absl::StatusCode::kOk) ||
      value > static_cast<int>(absl::StatusCode::kUnauthenticated)) {
    ThrowStatus(absl::InvalidArgumentError("status code is not canonical"));
  }
  return static_cast<absl::StatusCode>(value);
}

nlohmann::json JsonFromPython(const py::handle& value) {
  try {
    const auto encoded =
        py::module_::import("json").attr("dumps")(value).cast<std::string>();
    return nlohmann::json::parse(encoded);
  } catch (py::error_already_set& error) {
    ThrowStatus(StatusFromPythonException(&error));
  } catch (const std::exception& error) {
    ThrowStatus(absl::InvalidArgumentError(error.what()));
  }
}

py::object JsonToPython(const nlohmann::json& value) {
  return py::module_::import("json").attr("loads")(value.dump());
}

absl::Status MakeNativeStatus(int code, const std::string& message,
                              const py::handle& details) {
  nlohmann::json converted = JsonFromPython(details);
  if (!converted.is_array()) {
    ThrowStatus(absl::InvalidArgumentError("status details must be a list"));
  }
  return MakeStatus(CanonicalStatusCode(code), message, converted);
}

void SetStatusCode(NativeStatus* absl_nonnull status, int code) {
  status->value() = MakeStatus(CanonicalStatusCode(code),
                               std::string(status->value().message()),
                               StatusDetails(status->value()));
}

void SetStatusMessage(NativeStatus* absl_nonnull status,
                      const std::string& message) {
  status->value() = MakeStatus(status->value().code(), message,
                               StatusDetails(status->value()));
}

void SetStatusDetails(NativeStatus* absl_nonnull status,
                      const PyLike<PyJsonArray>& details) {
  status->value() =
      MakeNativeStatus(static_cast<int>(status->value().code()),
                       std::string(status->value().message()), details);
}

}  // namespace

void BindStatus(py::module_* absl_nonnull module) {
  py::class_<NativeStatus>(*module, "Status", py::dynamic_attr())
      .def(py::init(
               [](int code, std::string message, const py::object& details) {
                 return NativeStatus(MakeNativeStatus(code, message, details));
               }),
           "Creates a status from a canonical code, message, and details list.",
           py::arg("code") = 0, py::arg("message") = "OK",
           py::arg("details") = py::list())
      .def_property(
          "code",
          [](const NativeStatus& status) -> PyStatusCode {
            return py::module_::import("symbian.status")
                .attr("StatusCode")(static_cast<int>(status.value().code()));
          },
          &SetStatusCode, "The canonical status code.")
      .def_property(
          "message",
          [](const NativeStatus& status) {
            return std::string(status.value().message());
          },
          &SetStatusMessage, "The human-readable status message.")
      .def_property(
          "details",
          [](const NativeStatus& status) -> PyJsonArray {
            return JsonToPython(StatusDetails(status.value()));
          },
          &SetStatusDetails, "The structured status details, as a list.")
      .def(
          "is_ok",
          [](const NativeStatus& status) { return status.value().ok(); },
          "Returns whether the status is OK (no error).")
      .def(
          "_as_dict",
          [](const NativeStatus& status) -> PyJsonObject {
            return JsonToPython(ValueOrThrow(StatusToJson(status.value())));
          },
          "Returns the status as a JSON-compatible dict.")
      .def(
          "_copy", [](const NativeStatus& status) { return status; },
          "Returns a copy of this status.")
      .def(
          "__eq__",
          [](const NativeStatus& left, const NativeStatus& right) {
            return left.value() == right.value() &&
                   StatusDetails(left.value()) == StatusDetails(right.value());
          },
          "Returns whether two statuses have equal code, message, and details.",
          py::arg("right"), py::is_operator())
      .def(
          "__str__",
          [](const NativeStatus& status) -> py::str {
            py::object code = py::module_::import("symbian.status")
                                  .attr("StatusCode")(
                                      static_cast<int>(status.value().code()));
            return py::str("{}: {}").attr("format")(
                code, std::string(status.value().message()));
          },
          "Returns a 'CODE: message' string form of the status.")
      .def(
          "__repr__",
          [](const NativeStatus& status) {
            return "Status(code=" +
                   std::to_string(static_cast<int>(status.value().code())) +
                   ", message='" + std::string(status.value().message()) + "')";
          },
          "Returns a debug representation of the status.");

  module->def("_status_roundtrip", [](const py::object& value) {
    return StatusToPython(StatusFromPython(value));
  });
  module->def("_status_or_value",
              [](const py::object& value, const std::string& output) {
                const absl::Status status = StatusFromPython(value);
                return ValueWithoutGil([&]() -> absl::StatusOr<std::string> {
                  if (!status.ok()) {
                    return status;
                  }
                  return output;
                });
              });
  module->def("_status_from_callback", [](const py::function& callback) {
    try {
      return StatusToPython(StatusFromPython(callback()));
    } catch (py::error_already_set& error) {
      return StatusToPython(StatusFromPythonException(&error));
    }
  });
  module->def("_absl_status_roundtrip", [](absl::Status status) {
    return py::google::DoNotThrowStatus(std::move(status));
  });
  module->def("status_code_from_http", [](int code) {
    return static_cast<int>(StatusCodeFromHttp(code));
  });
  module->def("status_code_to_http", [](int code) {
    return StatusCodeToHttp(CanonicalStatusCode(code));
  });
  module->def("status_code_from_websocket", [](int code) {
    if (code < 0 || code > UINT16_MAX) {
      return static_cast<int>(absl::StatusCode::kUnknown);
    }
    return static_cast<int>(
        StatusCodeFromWebSocket(static_cast<std::uint16_t>(code)));
  });
  module->def("status_code_to_websocket", [](int code) {
    return StatusCodeToWebSocket(CanonicalStatusCode(code));
  });
}
}  // namespace symbian::python
