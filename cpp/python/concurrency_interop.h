/*
 * Copyright 2026 The A11 Authors and the Symbian SDK Authors.
 * Licensed under the Apache License, Version 2.0.
 * Adapted from A11 cpp/python/interop.h at
 * fcccb8cb6e1e67d7ba0822ac14cece9ee4c7091b.
 */

#ifndef SYMBIAN_PYTHON_CONCURRENCY_INTEROP_H_
#define SYMBIAN_PYTHON_CONCURRENCY_INTEROP_H_

#include <atomic>
#include <cstddef>
#include <exception>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include <Python.h>
#include <absl/base/no_destructor.h>
#include <absl/synchronization/mutex.h>
#include <pybind11/pybind11.h>

#include "python/status_interop.h"
#include "symbian/concurrency/future.h"

namespace symbian::python {

inline bool InterpreterIsGoingAway() {
  if (Py_IsInitialized() == 0) {
    return true;
  }
#if PY_VERSION_HEX >= 0x030D0000
  return Py_IsFinalizing() != 0;
#else
  return _Py_IsFinalizing() != 0;
#endif
}

// Destructors may retire Python references but must never acquire the GIL.
// The next binding entry point or the atexit hook drains them with GIL held.
// References left at interpreter teardown intentionally remain unreleased.
class DeferredPythonRefs {
 public:
  static void Retire(PyObject* object) {
    if (object == nullptr) {
      return;
    }
    if (Py_IsInitialized() != 0 && PyGILState_Check() != 0) {
      Py_DECREF(object);
      return;
    }
    absl::MutexLock lock(Mutex());
    Pending().push_back(object);
    Size().store(Pending().size(), std::memory_order_relaxed);
  }

  // The caller must hold the GIL.
  static void Drain() {
    if (Size().load(std::memory_order_relaxed) == 0) {
      return;
    }
    std::vector<PyObject*> objects;
    {
      absl::MutexLock lock(Mutex());
      objects.swap(Pending());
      Size().store(0, std::memory_order_relaxed);
    }
    for (PyObject* object : objects) {
      Py_DECREF(object);
    }
  }

  static size_t PendingCount() {
    return Size().load(std::memory_order_relaxed);
  }

 private:
  static std::atomic<size_t>& Size() {
    static absl::NoDestructor<std::atomic<size_t>> count{0};
    return *count;
  }

  static absl::Mutex& Mutex() {
    static absl::NoDestructor<absl::Mutex> mutex;
    return *mutex;
  }

  static std::vector<PyObject*>& Pending() {
    static absl::NoDestructor<std::vector<PyObject*>> objects;
    return *objects;
  }
};

// A11's PythonReferences pattern: retain the event loop and Python future as
// raw references so native completion may end on any worker without running a
// py::object destructor there. The ref holder itself never enters Python.
class PythonFutureReferences {
 public:
  PythonFutureReferences(pybind11::handle loop, pybind11::handle future)
      : loop_(loop.inc_ref().ptr()),
        future_(future.inc_ref().ptr()),
        loop_thread_(PyThread_get_thread_ident()) {}

  ~PythonFutureReferences() {
    DeferredPythonRefs::Retire(std::exchange(loop_, nullptr));
    DeferredPythonRefs::Retire(std::exchange(future_, nullptr));
  }

  PythonFutureReferences(const PythonFutureReferences&) = delete;
  PythonFutureReferences& operator=(const PythonFutureReferences&) = delete;

  pybind11::object loop() const {
    return pybind11::reinterpret_borrow<pybind11::object>(loop_);
  }

  pybind11::object future() const {
    return pybind11::reinterpret_borrow<pybind11::object>(future_);
  }

  bool OnLoopThread() const {
    return PyThread_get_thread_ident() == loop_thread_;
  }

 private:
  PyObject* loop_;
  PyObject* future_;
  unsigned long loop_thread_;
};

// The running asyncio loop is captured at registration. OnReady may complete
// inline while the GIL is released; its callback reacquires the GIL, then
// resolves on that loop or posts with call_soon_threadsafe. Converter touches
// Python only after the GIL is held and must be copy-constructible because the
// staged Future stores std::function callbacks.
template <typename T, typename Converter>
pybind11::object FutureToPython(symbian::concurrency::Future<T> future,
                                Converter converter) {
  namespace py = pybind11;
  static_assert(std::is_copy_constructible_v<Converter>);
  DeferredPythonRefs::Drain();
  py::object loop = py::module_::import("asyncio").attr("get_running_loop")();
  py::object python_future = loop.attr("create_future")();
  auto references =
      std::make_shared<PythonFutureReferences>(loop, python_future);
  python_future.attr("add_done_callback")(
      py::cpp_function([future](py::object done) mutable {
        if (!done.attr("cancelled")().cast<bool>()) {
          return;
        }
        py::gil_scoped_release release;
        (void)future.Cancel();
      }));
  auto callback = [references, converter = std::move(converter)](
                      const absl::StatusOr<T>& result) mutable {
    if (InterpreterIsGoingAway()) {
      return;
    }
    py::gil_scoped_acquire acquire;
    DeferredPythonRefs::Drain();
    try {
      py::object value = py::none();
      py::object error = py::none();
      if (result.ok()) {
        try {
          value = converter(*result);
        } catch (py::error_already_set& raised) {
          error = raised.value();
          raised.restore();
          PyErr_Clear();
        } catch (const std::exception& raised) {
          error = py::module_::import("builtins")
                      .attr("RuntimeError")(raised.what());
        }
      } else {
        error = StatusException(result.status());
      }
      py::cpp_function completion([references, value = std::move(value),
                                   error = std::move(error)]() mutable {
        py::object target = references->future();
        if (target.attr("done")().cast<bool>()) {
          return;
        }
        if (error.is_none()) {
          target.attr("set_result")(value);
        } else {
          target.attr("set_exception")(error);
        }
      });
      if (references->OnLoopThread()) {
        completion();
      } else {
        references->loop().attr("call_soon_threadsafe")(completion);
      }
    } catch (const py::error_already_set&) {
      // A closed event loop no longer has a waiter to receive an error.
      PyErr_Clear();
    }
  };
  {
    py::gil_scoped_release release;
    future.OnReady(std::move(callback));
  }
  return python_future;
}

void InstallPythonSchedulerParkGuard();
void BindConcurrencyInterop(pybind11::module_& module);

}  // namespace symbian::python

#endif  // SYMBIAN_PYTHON_CONCURRENCY_INTEROP_H_
