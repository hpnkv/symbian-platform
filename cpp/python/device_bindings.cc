#include <cstdint>
#include <memory>
#include <string>

#include <absl/status/statusor.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "python/concurrency_interop.h"
#include "python/status_interop.h"
#include "symbian/device/transport.h"
#include "symbian/device/usb.h"

namespace symbian::python {

pybind11::object UsbCompletionToPython(
    const device::UsbCompletion& completion) {
  return pybind11::cast(completion);
}

pybind11::object TransferFutureToPython(
    absl::StatusOr<symbian::concurrency::Future<device::UsbCompletion>>
        submitted) {
  symbian::concurrency::Future<device::UsbCompletion> future =
      submitted.ok()
          ? std::move(*submitted)
          : symbian::concurrency::FailedFuture<device::UsbCompletion>(
                submitted.status());
  return FutureToPython(std::move(future), UsbCompletionToPython);
}

void RequireRunningLoop() {
  pybind11::module_::import("asyncio").attr("get_running_loop")();
}

void BindDevice(pybind11::module_& module) {
  pybind11::class_<device::UsbEndpointDescriptor>(module,
                                                  "UsbEndpointDescriptor")
      .def_readonly("address", &device::UsbEndpointDescriptor::address)
      .def_readonly("direction", &device::UsbEndpointDescriptor::direction)
      .def_readonly("transfer", &device::UsbEndpointDescriptor::transfer)
      .def_readonly("max_packet_bytes",
                    &device::UsbEndpointDescriptor::max_packet_bytes);
  pybind11::class_<device::UsbInterfaceDescriptor>(module,
                                                   "UsbInterfaceDescriptor")
      .def_readonly("number", &device::UsbInterfaceDescriptor::number)
      .def_readonly("alternate_setting",
                    &device::UsbInterfaceDescriptor::alternate_setting)
      .def_readonly("class_code", &device::UsbInterfaceDescriptor::class_code)
      .def_readonly("subclass_code",
                    &device::UsbInterfaceDescriptor::subclass_code)
      .def_readonly("protocol_code",
                    &device::UsbInterfaceDescriptor::protocol_code)
      .def_readonly("endpoints", &device::UsbInterfaceDescriptor::endpoints);
  pybind11::class_<device::UsbCdcUnion>(module, "UsbCdcUnion")
      .def_readonly("master", &device::UsbCdcUnion::master)
      .def_readonly("slaves", &device::UsbCdcUnion::slaves);
  pybind11::class_<device::UsbInterfaceAssociation>(module,
                                                    "UsbInterfaceAssociation")
      .def_readonly("first_interface",
                    &device::UsbInterfaceAssociation::first_interface)
      .def_readonly("interface_count",
                    &device::UsbInterfaceAssociation::interface_count)
      .def_readonly("class_code", &device::UsbInterfaceAssociation::class_code)
      .def_readonly("subclass_code",
                    &device::UsbInterfaceAssociation::subclass_code)
      .def_readonly("protocol_code",
                    &device::UsbInterfaceAssociation::protocol_code);
  pybind11::class_<device::MtpDeviceInfo>(module, "MtpDeviceInfo")
      .def_readonly("standard_version",
                    &device::MtpDeviceInfo::standard_version)
      .def_readonly("vendor_extension_id",
                    &device::MtpDeviceInfo::vendor_extension_id)
      .def_readonly("vendor_extension_version",
                    &device::MtpDeviceInfo::vendor_extension_version)
      .def_readonly("vendor_extension_description",
                    &device::MtpDeviceInfo::vendor_extension_description)
      .def_readonly("functional_mode", &device::MtpDeviceInfo::functional_mode)
      .def_readonly("supported_operation_codes",
                    &device::MtpDeviceInfo::supported_operation_codes)
      .def_readonly("manufacturer", &device::MtpDeviceInfo::manufacturer)
      .def_readonly("model", &device::MtpDeviceInfo::model)
      .def_readonly("device_version", &device::MtpDeviceInfo::device_version);
  pybind11::class_<device::MtpObjectInfo>(module, "MtpObjectInfo")
      .def_readonly("handle", &device::MtpObjectInfo::handle)
      .def_readonly("format_code", &device::MtpObjectInfo::format_code)
      .def_readonly("size_bytes", &device::MtpObjectInfo::size_bytes)
      .def_readonly("name", &device::MtpObjectInfo::name)
      .def_readonly("error", &device::MtpObjectInfo::error)
      .def_readonly("response_code", &device::MtpObjectInfo::response_code);
  pybind11::class_<device::MtpStorageInfo>(module, "MtpStorageInfo")
      .def_readonly("id", &device::MtpStorageInfo::id)
      .def_readonly("storage_type", &device::MtpStorageInfo::storage_type)
      .def_readonly("filesystem_type", &device::MtpStorageInfo::filesystem_type)
      .def_readonly("access_capability",
                    &device::MtpStorageInfo::access_capability)
      .def_readonly("total_bytes", &device::MtpStorageInfo::total_bytes)
      .def_readonly("free_bytes", &device::MtpStorageInfo::free_bytes)
      .def_readonly("free_images", &device::MtpStorageInfo::free_images)
      .def_readonly("description", &device::MtpStorageInfo::description)
      .def_readonly("volume_label", &device::MtpStorageInfo::volume_label)
      .def_readonly("root_object_count",
                    &device::MtpStorageInfo::root_object_count)
      .def_readonly("root_objects", &device::MtpStorageInfo::root_objects)
      .def_readonly("root_listing_error",
                    &device::MtpStorageInfo::root_listing_error)
      .def_readonly("root_listing_response_code",
                    &device::MtpStorageInfo::root_listing_response_code)
      .def_readonly("error", &device::MtpStorageInfo::error)
      .def_readonly("response_code", &device::MtpStorageInfo::response_code);
  pybind11::class_<device::UsbProbe>(module, "UsbProbe")
      .def_readonly("state", &device::UsbProbe::state)
      .def_readonly("backend", &device::UsbProbe::backend)
      .def_readonly("detail", &device::UsbProbe::detail)
      .def_readonly("response_code", &device::UsbProbe::response_code)
      .def_readonly("scope", &device::UsbProbe::scope)
      .def_readonly("identity_basis", &device::UsbProbe::identity_basis)
      .def_readonly("configuration", &device::UsbProbe::configuration)
      .def_readonly("interfaces", &device::UsbProbe::interfaces)
      .def_readonly("cdc_unions", &device::UsbProbe::cdc_unions)
      .def_readonly("interface_associations",
                    &device::UsbProbe::interface_associations)
      .def_readonly("transport", &device::UsbProbe::transport)
      .def_property_readonly(
          "interface",
          [](const device::UsbProbe& probe) { return probe.interface_number; })
      .def_readonly("device_info", &device::UsbProbe::device_info)
      .def_readonly("storage", &device::UsbProbe::storage)
      .def_readonly("storage_count", &device::UsbProbe::storage_count)
      .def_readonly("session_closed", &device::UsbProbe::session_closed)
      .def_readonly("target", &device::UsbProbe::target)
      .def_readonly("connection_id_present",
                    &device::UsbProbe::connection_id_present)
      .def_readonly("disconnected", &device::UsbProbe::disconnected)
      .def_readonly("disconnect_error", &device::UsbProbe::disconnect_error)
      .def_readonly("disconnect_response_code",
                    &device::UsbProbe::disconnect_response_code)
      .def_readonly("alternate_restored", &device::UsbProbe::alternate_restored)
      .def_readonly("interface_released", &device::UsbProbe::interface_released)
      .def_readonly("response_bytes", &device::UsbProbe::response_bytes);
  pybind11::class_<device::UsbDeviceDescriptor>(module, "UsbDeviceDescriptor")
      .def_readonly("vendor_id", &device::UsbDeviceDescriptor::vendor_id)
      .def_readonly("product_id", &device::UsbDeviceDescriptor::product_id)
      .def_readonly("bus", &device::UsbDeviceDescriptor::bus)
      .def_readonly("address", &device::UsbDeviceDescriptor::address)
      .def_readonly("ports", &device::UsbDeviceDescriptor::ports)
      .def_readonly("device_class", &device::UsbDeviceDescriptor::device_class)
      .def_readonly("configuration_count",
                    &device::UsbDeviceDescriptor::configuration_count);
  pybind11::class_<device::MtpStageResult>(module, "MtpStageResult")
      .def_readonly("storage_id", &device::MtpStageResult::storage_id)
      .def_readonly("object_handle", &device::MtpStageResult::object_handle)
      .def_readonly("name", &device::MtpStageResult::name)
      .def_readonly("copied", &device::MtpStageResult::copied);
  pybind11::class_<device::UsbPollFd>(module, "UsbPollFd")
      .def_readonly("fd", &device::UsbPollFd::fd)
      .def_readonly("events", &device::UsbPollFd::events);
  pybind11::class_<device::UsbCompletion>(module, "UsbCompletion")
      .def_readonly("id", &device::UsbCompletion::id)
      .def_readonly("status", &device::UsbCompletion::status)
      .def_readonly("actual_length", &device::UsbCompletion::actual_length)
      .def_property_readonly("data",
                             [](const device::UsbCompletion& completion) {
                               return pybind11::bytes(completion.data);
                             });
  module.def(
      "inspect_usb_native",
      [](uint16_t vendor, uint16_t product, const std::string& anchor,
         const std::string& operation, uint32_t limit) {
        return ValueWithoutGil([&] {
          return device::InspectUsb(vendor, product, anchor, operation, limit);
        });
      },
      pybind11::arg("vendor"), pybind11::arg("product"),
      pybind11::arg("anchor"), pybind11::arg("operation"),
      pybind11::arg("limit") = 0);
  module.def(
      "stage_mtp_sis_native",
      [](uint16_t vendor, uint16_t product, const std::string& anchor,
         const std::string& package_path, const std::string& filename,
         const std::string& expected_sha256) {
        return ValueWithoutGil([&] {
          return device::StageMtpSis(vendor, product, anchor, package_path,
                                     filename, expected_sha256);
        });
      },
      pybind11::arg("vendor"), pybind11::arg("product"),
      pybind11::arg("anchor"), pybind11::arg("package_path"),
      pybind11::arg("filename"), pybind11::arg("expected_sha256"));
  module.def("list_usb_devices_native", [] {
    return ValueWithoutGil([] { return device::ListUsbDevices(); });
  });
  pybind11::class_<device::UsbSession, std::shared_ptr<device::UsbSession>>(
      module, "UsbSession")
      .def_static(
          "open",
          [](uint16_t vendor, uint16_t product, const std::string& anchor) {
            return ValueWithoutGil([&] {
              return device::UsbSession::Open(vendor, product, anchor);
            });
          },
          pybind11::arg("vendor"), pybind11::arg("product"),
          pybind11::arg("serial_anchor"))
      .def("descriptors",
           [](device::UsbSession& session) {
             return ValueWithoutGil([&] { return session.Descriptors(); });
           })
      .def("claim",
           [](device::UsbSession& session, int number) {
             CallWithoutGil([&] { return session.Claim(number); });
           })
      .def("set_alternate",
           [](device::UsbSession& session, int number, int alternate) {
             CallWithoutGil(
                 [&] { return session.SetAlternate(number, alternate); });
           })
      .def("release",
           [](device::UsbSession& session, int number) {
             CallWithoutGil([&] { return session.Release(number); });
           })
      .def(
          "bulk_in",
          [](device::UsbSession& session, uint8_t endpoint, int length,
             int timeout_ms) {
            return pybind11::bytes(ValueWithoutGil(
                [&] { return session.BulkIn(endpoint, length, timeout_ms); }));
          },
          pybind11::arg("endpoint"), pybind11::arg("length"),
          pybind11::arg("timeout_ms") = 1500)
      .def(
          "bulk_out",
          [](device::UsbSession& session, uint8_t endpoint,
             const pybind11::bytes& data, int timeout_ms) {
            std::string bytes = data;
            CallWithoutGil(
                [&] { return session.BulkOut(endpoint, bytes, timeout_ms); });
          },
          pybind11::arg("endpoint"), pybind11::arg("data"),
          pybind11::arg("timeout_ms") = 1500)
      .def(
          "interrupt_in",
          [](device::UsbSession& session, uint8_t endpoint, int length,
             int timeout_ms) {
            return pybind11::bytes(ValueWithoutGil([&] {
              return session.InterruptIn(endpoint, length, timeout_ms);
            }));
          },
          pybind11::arg("endpoint"), pybind11::arg("length"),
          pybind11::arg("timeout_ms") = 1500)
      .def(
          "interrupt_out",
          [](device::UsbSession& session, uint8_t endpoint,
             const pybind11::bytes& data, int timeout_ms) {
            std::string bytes = data;
            CallWithoutGil([&] {
              return session.InterruptOut(endpoint, bytes, timeout_ms);
            });
          },
          pybind11::arg("endpoint"), pybind11::arg("data"),
          pybind11::arg("timeout_ms") = 1500)
      .def(
          "control_in",
          [](device::UsbSession& session, uint8_t request_type, uint8_t request,
             uint16_t value, uint16_t index, int length, int timeout_ms) {
            return pybind11::bytes(ValueWithoutGil([&] {
              return session.ControlIn(request_type, request, value, index,
                                       length, timeout_ms);
            }));
          },
          pybind11::arg("request_type"), pybind11::arg("request"),
          pybind11::arg("value"), pybind11::arg("index"),
          pybind11::arg("length"), pybind11::arg("timeout_ms") = 1500)
      .def(
          "control_out",
          [](device::UsbSession& session, uint8_t request_type, uint8_t request,
             uint16_t value, uint16_t index, const pybind11::bytes& data,
             int timeout_ms) {
            std::string bytes = data;
            CallWithoutGil([&] {
              return session.ControlOut(request_type, request, value, index,
                                        bytes, timeout_ms);
            });
          },
          pybind11::arg("request_type"), pybind11::arg("request"),
          pybind11::arg("value"), pybind11::arg("index"), pybind11::arg("data"),
          pybind11::arg("timeout_ms") = 1500)
      .def(
          "submit_bulk_in",
          [](device::UsbSession& session, uint8_t endpoint, int length,
             int timeout_ms) {
            return ValueWithoutGil([&] {
              return session.SubmitBulk(endpoint, "", length, timeout_ms);
            });
          },
          pybind11::arg("endpoint"), pybind11::arg("length"),
          pybind11::arg("timeout_ms") = 1500)
      .def(
          "submit_bulk_out",
          [](device::UsbSession& session, uint8_t endpoint,
             const pybind11::bytes& data, int timeout_ms) {
            std::string bytes = data;
            return ValueWithoutGil([&] {
              return session.SubmitBulk(endpoint, bytes, 0, timeout_ms);
            });
          },
          pybind11::arg("endpoint"), pybind11::arg("data"),
          pybind11::arg("timeout_ms") = 1500)
      .def(
          "submit_interrupt_in",
          [](device::UsbSession& session, uint8_t endpoint, int length,
             int timeout_ms) {
            return ValueWithoutGil([&] {
              return session.SubmitInterrupt(endpoint, "", length, timeout_ms);
            });
          },
          pybind11::arg("endpoint"), pybind11::arg("length"),
          pybind11::arg("timeout_ms") = 1500)
      .def(
          "submit_interrupt_out",
          [](device::UsbSession& session, uint8_t endpoint,
             const pybind11::bytes& data, int timeout_ms) {
            std::string bytes = data;
            return ValueWithoutGil([&] {
              return session.SubmitInterrupt(endpoint, bytes, 0, timeout_ms);
            });
          },
          pybind11::arg("endpoint"), pybind11::arg("data"),
          pybind11::arg("timeout_ms") = 1500)
      .def(
          "submit_control_in",
          [](device::UsbSession& session, uint8_t request_type, uint8_t request,
             uint16_t value, uint16_t index, int length, int timeout_ms) {
            return ValueWithoutGil([&] {
              return session.SubmitControl(request_type, request, value, index,
                                           "", length, timeout_ms);
            });
          },
          pybind11::arg("request_type"), pybind11::arg("request"),
          pybind11::arg("value"), pybind11::arg("index"),
          pybind11::arg("length"), pybind11::arg("timeout_ms") = 1500)
      .def(
          "submit_control_out",
          [](device::UsbSession& session, uint8_t request_type, uint8_t request,
             uint16_t value, uint16_t index, const pybind11::bytes& data,
             int timeout_ms) {
            std::string bytes = data;
            return ValueWithoutGil([&] {
              return session.SubmitControl(request_type, request, value, index,
                                           bytes, 0, timeout_ms);
            });
          },
          pybind11::arg("request_type"), pybind11::arg("request"),
          pybind11::arg("value"), pybind11::arg("index"), pybind11::arg("data"),
          pybind11::arg("timeout_ms") = 1500)
      .def(
          "bulk_in_future",
          [](device::UsbSession& session, uint8_t endpoint, int length,
             int timeout_ms) {
            RequireRunningLoop();
            auto submitted = [&] {
              pybind11::gil_scoped_release release;
              return session.SubmitBulkFuture(endpoint, "", length, timeout_ms);
            }();
            return TransferFutureToPython(std::move(submitted));
          },
          pybind11::arg("endpoint"), pybind11::arg("length"),
          pybind11::arg("timeout_ms") = 1500)
      .def(
          "bulk_out_future",
          [](device::UsbSession& session, uint8_t endpoint,
             const pybind11::bytes& data, int timeout_ms) {
            RequireRunningLoop();
            std::string bytes = data;
            auto submitted = [&] {
              pybind11::gil_scoped_release release;
              return session.SubmitBulkFuture(endpoint, bytes, 0, timeout_ms);
            }();
            return TransferFutureToPython(std::move(submitted));
          },
          pybind11::arg("endpoint"), pybind11::arg("data"),
          pybind11::arg("timeout_ms") = 1500)
      .def(
          "interrupt_in_future",
          [](device::UsbSession& session, uint8_t endpoint, int length,
             int timeout_ms) {
            RequireRunningLoop();
            auto submitted = [&] {
              pybind11::gil_scoped_release release;
              return session.SubmitInterruptFuture(endpoint, "", length,
                                                   timeout_ms);
            }();
            return TransferFutureToPython(std::move(submitted));
          },
          pybind11::arg("endpoint"), pybind11::arg("length"),
          pybind11::arg("timeout_ms") = 1500)
      .def(
          "interrupt_out_future",
          [](device::UsbSession& session, uint8_t endpoint,
             const pybind11::bytes& data, int timeout_ms) {
            RequireRunningLoop();
            std::string bytes = data;
            auto submitted = [&] {
              pybind11::gil_scoped_release release;
              return session.SubmitInterruptFuture(endpoint, bytes, 0,
                                                   timeout_ms);
            }();
            return TransferFutureToPython(std::move(submitted));
          },
          pybind11::arg("endpoint"), pybind11::arg("data"),
          pybind11::arg("timeout_ms") = 1500)
      .def(
          "control_in_future",
          [](device::UsbSession& session, uint8_t request_type, uint8_t request,
             uint16_t value, uint16_t index, int length, int timeout_ms) {
            RequireRunningLoop();
            auto submitted = [&] {
              pybind11::gil_scoped_release release;
              return session.SubmitControlFuture(request_type, request, value,
                                                 index, "", length, timeout_ms);
            }();
            return TransferFutureToPython(std::move(submitted));
          },
          pybind11::arg("request_type"), pybind11::arg("request"),
          pybind11::arg("value"), pybind11::arg("index"),
          pybind11::arg("length"), pybind11::arg("timeout_ms") = 1500)
      .def(
          "control_out_future",
          [](device::UsbSession& session, uint8_t request_type, uint8_t request,
             uint16_t value, uint16_t index, const pybind11::bytes& data,
             int timeout_ms) {
            RequireRunningLoop();
            std::string bytes = data;
            auto submitted = [&] {
              pybind11::gil_scoped_release release;
              return session.SubmitControlFuture(request_type, request, value,
                                                 index, bytes, 0, timeout_ms);
            }();
            return TransferFutureToPython(std::move(submitted));
          },
          pybind11::arg("request_type"), pybind11::arg("request"),
          pybind11::arg("value"), pybind11::arg("index"), pybind11::arg("data"),
          pybind11::arg("timeout_ms") = 1500)
      .def("cancel",
           [](device::UsbSession& session, uint64_t id) {
             CallWithoutGil([&] { return session.Cancel(id); });
           })
      .def(
          "handle_events",
          [](device::UsbSession& session, int timeout_ms) {
            auto completed = ValueWithoutGil(
                [&] { return session.HandleEvents(timeout_ms); });
            return completed;
          },
          pybind11::arg("timeout_ms") = 0)
      .def("poll_fds",
           [](device::UsbSession& session) {
             auto descriptors =
                 ValueWithoutGil([&] { return session.PollFileDescriptors(); });
             return descriptors;
           })
      .def("next_timeout_ms",
           [](device::UsbSession& session) {
             return ValueWithoutGil([&] { return session.NextTimeoutMs(); });
           })
      .def("__enter__",
           [](std::shared_ptr<device::UsbSession> session) { return session; })
      .def("__exit__",
           [](device::UsbSession& session, pybind11::object, pybind11::object,
              pybind11::object) {
             CallWithoutGil([&] { return session.Close(); });
           })
      .def("close", [](device::UsbSession& session) {
        CallWithoutGil([&] { return session.Close(); });
      });
}
}  // namespace symbian::python
