#ifndef SYMBIAN_PYTHON_DEVICE_BINDINGS_H_
#define SYMBIAN_PYTHON_DEVICE_BINDINGS_H_
#include <pybind11/pybind11.h>

namespace symbian::python {
void BindDevice(pybind11::module_& module);
}
#endif
