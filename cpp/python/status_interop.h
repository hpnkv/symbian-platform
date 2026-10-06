/*
 * Copyright 2026 The A11 Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SYMBIAN_PYTHON_STATUS_INTEROP_H_
#define SYMBIAN_PYTHON_STATUS_INTEROP_H_

#include <utility>

#include <absl/base/nullability.h>
#include <absl/status/status.h>
#include <absl/status/statusor.h>
#include <pybind11/pybind11.h>
#include <pybind11/typing.h>

namespace symbian::python {
namespace py = pybind11;

class NativeStatus {
 public:
  NativeStatus() = default;

  explicit NativeStatus(absl::Status value) : value_(std::move(value)) {}

  [[nodiscard]] const absl::Status& value() const { return value_; }

  [[nodiscard]] absl::Status& value() { return value_; }

 private:
  absl::Status value_;
};

template <typename T>
class PyLike : public py::object {
  PYBIND11_OBJECT_DEFAULT(PyLike, object, PyObject_Type)
  using object::object;
};

class PyStatusCode : public py::object {
  PYBIND11_OBJECT_DEFAULT(PyStatusCode, object, PyObject_Type)
  using object::object;
};

using PyJsonObject = py::typing::Dict<py::str, py::object>;
using PyJsonArray = py::typing::List<py::object>;

absl::Status StatusFromPython(const py::handle& value);
py::object StatusToPython(const absl::Status& status);
absl::Status StatusFromPythonException(
    py::error_already_set* absl_nonnull error);
void BindStatus(py::module_* absl_nonnull module);

py::object StatusException(const absl::Status& status);
[[noreturn]] void ThrowStatus(const absl::Status& status);

template <typename T>
T ValueOrThrow(absl::StatusOr<T> value) {
  if (!value.ok()) {
    ThrowStatus(value.status());
  }
  T result = std::move(value).value();
  return result;
}

/// Raises the Python status exception for @p status unless it is OK.
inline void ThrowIfNotOk(const absl::Status& status) {
  if (!status.ok()) {
    ThrowStatus(status);
  }
}

/**
 * @brief Runs a blocking native operation with the GIL released.
 *
 * **Releasing is not an optimisation, it is required.** A call that blocks on
 * the libuv loop (`RunOnUv`/`RunStatusOnUv` -> `Future::Await`) is completed by
 * the uv thread, and completing it means touching Python objects --
 * `FutureToPython` callbacks, `py::object` destructors -- which needs the GIL.
 * A caller that blocked while holding it would deadlock the loop.
 *
 * Any argument conversion that touches Python must therefore happen *before*
 * the call, while the GIL is still held.
 */
template <typename Operation>
void CallWithoutGil(Operation&& operation) {
  absl::Status status;
  {
    py::gil_scoped_release release;
    status = std::forward<Operation>(operation)();
  }
  ThrowIfNotOk(status);
}

/// As CallWithoutGil, for an operation yielding an `absl::StatusOr<T>`; the
/// value is unwrapped, or thrown, with the GIL re-held.
template <typename Operation>
auto ValueWithoutGil(Operation&& operation) {
  auto result = [&] {
    py::gil_scoped_release release;
    return std::forward<Operation>(operation)();
  }();
  return ValueOrThrow(std::move(result));
}

}  // namespace symbian::python

PYBIND11_NAMESPACE_BEGIN(PYBIND11_NAMESPACE)
PYBIND11_NAMESPACE_BEGIN(detail)

template <typename T>
struct handle_type_name<symbian::python::PyLike<T>> {
  static constexpr auto name = make_caster<T>::name;
};

template <>
struct handle_type_name<symbian::python::PyStatusCode> {
  static constexpr auto name = const_name("symbian.status.StatusCode");
};
PYBIND11_NAMESPACE_END(detail)
PYBIND11_NAMESPACE_END(PYBIND11_NAMESPACE)
#endif  // SYMBIAN_PYTHON_STATUS_INTEROP_H_
