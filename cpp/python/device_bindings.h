#ifndef SYMBIAN_PYTHON_DEVICE_BINDINGS_H_
#define SYMBIAN_PYTHON_DEVICE_BINDINGS_H_

#include <absl/base/nullability.h>
#include <pybind11/pybind11.h>

namespace symbian::python {
void BindDevice(pybind11::module_* absl_nonnull module);
}
#endif
