// Copyright 2026 The A11 Authors and the Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.
// Adapted from A11 cpp/python/module.cc and interop.h at
// fcccb8cb6e1e67d7ba0822ac14cece9ee4c7091b.

#include "python/concurrency_interop.h"

#include <thread>

#include <absl/base/nullability.h>
#include <absl/time/time.h>

#include "thread/boost_primitives.h"
#include "thread/executor.h"
#include "thread/fiber.h"

namespace symbian::python {
namespace py = pybind11;

void InstallPythonSchedulerParkGuard() {
  thread::SetSchedulerParkGuard(thread::SchedulerParkGuard{
      .release = []() -> void* {
        if (PyGILState_Check() == 0 || InterpreterIsGoingAway()) {
          return nullptr;
        }
        return PyEval_SaveThread();
      },
      .acquire =
          [](void* absl_nullable held) {
            if (held != nullptr && !InterpreterIsGoingAway()) {
              PyEval_RestoreThread(static_cast<PyThreadState*>(held));
            }
          },
  });
}

void BindConcurrencyInterop(py::module_* absl_nonnull module) {
  py::module_::import("atexit").attr("register")(
      py::cpp_function([] { DeferredPythonRefs::Drain(); }));
  module->def("_thread_post_after_future", [](int milliseconds) {
    symbian::concurrency::Promise<int> promise;
    auto future = promise.future();
    py::object python_future =
        FutureToPython(future, [](int value) { return py::int_(value); });
    thread::PostAfter(absl::Milliseconds(milliseconds),
                      [promise = std::move(promise)]() mutable {
                        (void)promise.SetValue(17);
                      });
    return python_future;
  });
  module->def("_thread_park_probe", [](int milliseconds) {
    const bool gil_before = PyGILState_Check() != 0;
    thread::Fiber work(
        [milliseconds] { thread::SleepFor(absl::Milliseconds(milliseconds)); });
    const absl::Status joined = work.Join();
    return py::make_tuple(joined.ok(), gil_before, PyGILState_Check() != 0);
  });
  module->def("_deferred_ref_roundtrip", [](py::object object) {
    PyObject* absl_nonnull held = object.ptr();
    Py_INCREF(held);
    std::thread worker([held] { DeferredPythonRefs::Retire(held); });
    worker.join();
    const size_t pending = DeferredPythonRefs::PendingCount();
    DeferredPythonRefs::Drain();
    return py::make_tuple(pending, DeferredPythonRefs::PendingCount());
  });
}

}  // namespace symbian::python
