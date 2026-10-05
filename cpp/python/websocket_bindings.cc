// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.
#include <array>
#include <string>

#include <openssl/rand.h>
#include <pybind11/pybind11.h>

#include "python/status_interop.h"
#include "symbian/websocket/websocket.h"

namespace symbian::python {
void BindWebSocket(pybind11::module_& module) {
  namespace py = pybind11;
  using websocket::WebSocket;
  py::class_<WebSocket>(module, "WebSocketCodec")
      .def(py::init([](bool server, const std::string& path,
                       std::size_t maximum_message_bytes) {
             return ValueWithoutGil([&] {
               websocket::Options options;
               options.path = path;
               options.maximum_message_bytes = maximum_message_bytes;
               options.mask_provider =
                   []() -> absl::StatusOr<std::array<std::uint8_t, 4>> {
                 std::array<std::uint8_t, 4> mask{};
                 if (RAND_bytes(mask.data(), static_cast<int>(mask.size())) !=
                     1) {
                   return absl::UnavailableError(
                       "WebSocket masking entropy unavailable");
                 }
                 return mask;
               };
               return WebSocket::Create(
                   server ? websocket::Role::kServer : websocket::Role::kClient,
                   std::move(options));
             });
           }),
           py::arg("server") = false, py::arg("path") = "/symbian-agent",
           py::arg("maximum_message_bytes") = 4100)
      .def("feed",
           [](WebSocket& codec, const py::bytes& input) {
             const std::string bytes = input;
             CallWithoutGil([&] { return codec.Feed(bytes); });
           })
      .def("take_output",
           [](WebSocket& codec) {
             return py::bytes(
                 ValueWithoutGil([&] { return codec.TakeOutput(); }));
           })
      .def("send",
           [](WebSocket& codec, const py::bytes& input) {
             const std::string bytes = input;
             CallWithoutGil([&] { return codec.Send(bytes); });
           })
      .def("receive",
           [](WebSocket& codec) -> py::object {
             auto result = ValueWithoutGil([&] { return codec.Receive(); });
             return result ? py::object(py::bytes(*result))
                           : py::object(py::none());
           })
      .def("close",
           [](WebSocket& codec) {
             CallWithoutGil([&] { return codec.Close(); });
           })
      .def("abort", &WebSocket::Abort, py::call_guard<py::gil_scoped_release>())
      .def_property_readonly("open", &WebSocket::open)
      .def_property_readonly("closed", &WebSocket::closed)
      .def_property_readonly("buffered_amount", &WebSocket::buffered_amount);
}
}  // namespace symbian::python
