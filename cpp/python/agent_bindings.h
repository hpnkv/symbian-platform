// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_PYTHON_AGENT_BINDINGS_H_
#define SYMBIAN_PYTHON_AGENT_BINDINGS_H_

#include <pybind11/pybind11.h>

namespace symbian::python {

void BindWebSocket(pybind11::module_& module);

void BindAgent(pybind11::module_& module);

}  // namespace symbian::python

#endif  // SYMBIAN_PYTHON_AGENT_BINDINGS_H_
