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

#include "python/status_interop.h"

#include <exception>
#include <string>

#include <nlohmann/json.hpp>
#include <pybind11_abseil/compat/status_from_py_exc.h>
#include <pybind11_abseil/status_casters.h>

#include "symbian/status/status.h"

namespace symbian::python {
namespace {
absl::StatusCode CanonicalStatusCode(int value) {
  if (value < static_cast<int>(absl::StatusCode::kOk) ||
      value > static_cast<int>(absl::StatusCode::kUnauthenticated)) {
    return absl::StatusCode::kUnknown;
  }
  return static_cast<absl::StatusCode>(value);
}

}  // namespace

absl::Status StatusFromPython(const py::handle& value) {
  try {
    if (py::isinstance<NativeStatus>(value)) {
      return value.cast<const NativeStatus&>().value();
    }
    py::object ground_status =
        py::module_::import("symbian.status").attr("Status");
    if (py::isinstance(value, ground_status)) {
      const int code = value.attr("code").cast<int>();
      const auto message = value.attr("message").cast<std::string>();
      py::object details_object = value.attr("details");
      auto details_json = py::module_::import("json")
                              .attr("dumps")(details_object)
                              .cast<std::string>();
      nlohmann::json details = nlohmann::json::parse(details_json);
      return MakeStatus(CanonicalStatusCode(code), message, details);
    }
    return value.cast<absl::Status>();
  } catch (py::error_already_set& error) {
    return StatusFromPythonException(error);
  } catch (const std::exception& error) {
    return absl::InvalidArgumentError(error.what());
  } catch (...) {
    return absl::InvalidArgumentError(
        "Converting a Python status raised an exception");
  }
}

py::object StatusToPython(const absl::Status& status) {
  py::gil_scoped_acquire acquire;
  return py::cast(NativeStatus(status));
}

absl::Status StatusFromPythonException(py::error_already_set& error) {
  try {
    py::object exception = error.value();
    py::object cancelled_error =
        py::module_::import("asyncio").attr("CancelledError");
    if (py::isinstance(exception, cancelled_error)) {
      error.restore();
      PyErr_Clear();
      return absl::CancelledError("Python awaitable was cancelled");
    }
    py::object status_exception =
        py::module_::import("symbian.status").attr("StatusException");
    if (py::isinstance(exception, status_exception)) {
      absl::Status status = StatusFromPython(exception.attr("status"));
      // Consume the fetched exception without reporting it as unraisable: it
      // is an expected, recoverable status crossing the language boundary.
      error.restore();
      PyErr_Clear();
      return status;
    }
    const auto message = py::str(exception).cast<std::string>();
    error.restore();
    PyErr_Clear();
    return absl::UnknownError(message);
  } catch (const py::error_already_set&) {
    PyErr_Clear();
  } catch (...) {
    PyErr_Clear();
  }
  error.restore();
  return pybind11_abseil::compat::StatusFromPyExcGivenErrOccurred();
}

py::object StatusException(const absl::Status& status) {
  py::gil_scoped_acquire acquire;
  return py::module_::import("symbian.status")
      .attr("StatusException")(StatusToPython(status));
}

[[noreturn]] void ThrowStatus(const absl::Status& status) {
  py::object exception = StatusException(status);
  PyErr_SetObject(reinterpret_cast<PyObject*>(Py_TYPE(exception.ptr())),
                  exception.ptr());
  throw py::error_already_set();
}

}  // namespace symbian::python
