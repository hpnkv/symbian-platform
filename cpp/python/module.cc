#include <string>

#include <absl/status/status.h>
#include <absl/status/statusor.h>
#include <pybind11/pybind11.h>

#include "symbian/analysis/elf.h"
#include "symbian/e32/e32.h"

namespace py = pybind11;

namespace {

void RaiseStatus(const absl::Status& status) {
  py::object error_type =
      py::module_::import("symbian.status").attr("StatusError");
  py::object error = error_type(static_cast<int>(status.code()),
                                std::string(status.message()));
  PyErr_SetObject(error_type.ptr(), error.ptr());
  throw py::error_already_set();
}

symbian::analysis::Elf32Header InspectElf32(const py::bytes& data) {
  const std::string bytes = data;
  absl::StatusOr<symbian::analysis::Elf32Header> result;
  {
    const py::gil_scoped_release release;
    result = symbian::analysis::InspectElf32(bytes);
  }
  if (!result.ok()) {
    RaiseStatus(result.status());
  }
  return *result;
}

py::bytes ConvertPicExecutable(const py::bytes& data, uint32_t uid3) {
  const std::string bytes = data;
  absl::StatusOr<std::string> result;
  {
    const py::gil_scoped_release release;
    result = symbian::e32::ConvertPicExecutable(bytes, uid3);
  }
  if (!result.ok()) {
    RaiseStatus(result.status());
  }
  return py::bytes(*result);
}

symbian::e32::ImageInfo InspectE32(const py::bytes& data) {
  const std::string bytes = data;
  absl::StatusOr<symbian::e32::ImageInfo> result;
  {
    const py::gil_scoped_release release;
    result = symbian::e32::InspectImage(bytes);
  }
  if (!result.ok()) {
    RaiseStatus(result.status());
  }
  return *result;
}

}  // namespace

PYBIND11_MODULE(_native, module) {
  using symbian::analysis::Elf32Header;
  module.doc() = "Stateless native Symbian analysis utilities.";
  py::class_<Elf32Header>(module, "Elf32Header",
                          "ELF32 metadata; not a loader acceptance result.")
      .def_readonly("type", &Elf32Header::type, "ELF object type.")
      .def_readonly("machine", &Elf32Header::machine, "ELF machine identifier.")
      .def_readonly("entry", &Elf32Header::entry, "ELF entry address.")
      .def_readonly("flags", &Elf32Header::flags, "Target-specific ELF flags.")
      .def_readonly("program_count", &Elf32Header::program_count,
                    "Number of program headers.")
      .def_readonly("section_count", &Elf32Header::section_count,
                    "Number of section headers.");
  module.def(
      "inspect_elf32", &InspectElf32, py::arg("data"),
      "Inspect complete ELF32 bytes, releasing the GIL for native work.");
  using symbian::e32::ImageInfo;
  py::class_<ImageInfo>(module, "E32ImageInfo",
                        "Experimental E32 metadata; no runtime verdict.")
      .def_readonly("uid3", &ImageInfo::uid3)
      .def_readonly("header_crc", &ImageInfo::header_crc)
      .def_readonly("flags", &ImageInfo::flags)
      .def_readonly("code_size", &ImageInfo::code_size)
      .def_readonly("code_base", &ImageInfo::code_base)
      .def_readonly("entry_offset", &ImageInfo::entry_offset)
      .def_readonly("secure_id", &ImageInfo::secure_id);
  module.def("convert_pic_executable", &ConvertPicExecutable, py::arg("data"),
             py::arg("uid3"), "Convert a restricted, retained-relocation ELF.");
  module.def("inspect_e32", &InspectE32, py::arg("data"),
             "Check the experimental E32 profile, releasing the GIL.");
}
