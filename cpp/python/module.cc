#include <array>
#include <string>

#include <absl/status/status.h>
#include <absl/status/statusor.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "symbian/analysis/elf.h"
#include "symbian/e32/e32.h"
#include "symbian/sdk/exports.h"
#include "symbian/sis/sis.h"

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

py::bytes ConvertImportedExecutable(const py::bytes& data,
                                    const std::vector<py::bytes>& proxies,
                                    uint32_t uid3) {
  const std::string bytes = data;
  std::vector<std::string> libraries;
  libraries.reserve(proxies.size());
  for (const auto& proxy : proxies) {
    libraries.emplace_back(proxy);
  }
  absl::StatusOr<std::string> result;
  {
    const py::gil_scoped_release release;
    result = symbian::e32::ConvertImportedExecutable(bytes, libraries, uid3);
  }
  if (!result.ok()) {
    RaiseStatus(result.status());
  }
  return py::bytes(*result);
}

py::bytes ConvertDll(const py::bytes& data, const py::bytes& definition,
                     const std::vector<py::bytes>& proxies, uint32_t uid3) {
  const std::string bytes = data;
  const std::string exports = definition;
  std::vector<std::string> libraries;
  libraries.reserve(proxies.size());
  for (const auto& proxy : proxies) {
    libraries.emplace_back(proxy);
  }
  absl::StatusOr<std::string> result;
  {
    const py::gil_scoped_release release;
    result = symbian::e32::ConvertDll(bytes, exports, libraries, uid3);
  }
  if (!result.ok()) {
    RaiseStatus(result.status());
  }
  return py::bytes(*result);
}

py::bytes BuildSis(const py::bytes& data, uint32_t uid, const std::string& name,
                   const std::string& vendor,
                   const std::string& executable_name,
                   const std::array<int32_t, 3>& version) {
  const std::string bytes = data;
  const symbian::sis::PackageOptions options{uid, name, vendor, executable_name,
                                             version};
  absl::StatusOr<std::string> result;
  {
    const py::gil_scoped_release release;
    result = symbian::sis::BuildPackage(bytes, options);
  }
  if (!result.ok())
    RaiseStatus(result.status());
  return py::bytes(*result);
}

symbian::sis::PackageInfo InspectSis(const py::bytes& data) {
  const std::string bytes = data;
  absl::StatusOr<symbian::sis::PackageInfo> result;
  {
    const py::gil_scoped_release release;
    result = symbian::sis::InspectPackage(bytes);
  }
  if (!result.ok())
    RaiseStatus(result.status());
  return *result;
}

std::vector<symbian::sdk::Export> ParseDef(const py::bytes& data) {
  const std::string bytes = data;
  absl::StatusOr<std::vector<symbian::sdk::Export>> result;
  {
    const py::gil_scoped_release release;
    result = symbian::sdk::ParseExports(bytes);
  }
  if (!result.ok())
    RaiseStatus(result.status());
  return *result;
}

symbian::sdk::ProxySources GenerateProxy(
    const py::bytes& data, const std::vector<std::string>& symbols,
    const std::string& soname, const std::string& target_dll) {
  const std::string bytes = data;
  absl::StatusOr<symbian::sdk::ProxySources> result;
  {
    const py::gil_scoped_release release;
    result = symbian::sdk::GenerateProxy(bytes, symbols, soname, target_dll);
  }
  if (!result.ok())
    RaiseStatus(result.status());
  return *result;
}

symbian::sdk::ProxyInfo InspectProxy(const py::bytes& data) {
  const std::string bytes = data;
  absl::StatusOr<symbian::sdk::ProxyInfo> result;
  {
    const py::gil_scoped_release release;
    result = symbian::sdk::InspectProxy(bytes);
  }
  if (!result.ok())
    RaiseStatus(result.status());
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
  using symbian::e32::ExportSlot;
  py::class_<ExportSlot>(module, "E32ExportSlot")
      .def_readonly("ordinal", &ExportSlot::ordinal)
      .def_readonly("address", &ExportSlot::address)
      .def_readonly("absent", &ExportSlot::absent);
  using symbian::e32::ImageInfo;
  using symbian::e32::ImportBlock;
  using symbian::e32::ImportSlot;
  py::class_<ImportSlot>(module, "E32ImportSlot")
      .def_readonly("code_offset", &ImportSlot::code_offset)
      .def_readonly("ordinal", &ImportSlot::ordinal);
  py::class_<ImportBlock>(module, "E32ImportBlock")
      .def_readonly("dll", &ImportBlock::dll)
      .def_readonly("slots", &ImportBlock::slots);
  py::class_<ImageInfo>(module, "E32ImageInfo",
                        "Experimental E32 metadata; no runtime verdict.")
      .def_readonly("uid3", &ImageInfo::uid3)
      .def_readonly("header_crc", &ImageInfo::header_crc)
      .def_readonly("flags", &ImageInfo::flags)
      .def_readonly("code_size", &ImageInfo::code_size)
      .def_readonly("code_base", &ImageInfo::code_base)
      .def_readonly("entry_offset", &ImageInfo::entry_offset)
      .def_readonly("secure_id", &ImageInfo::secure_id)
      .def_readonly("dll", &ImageInfo::dll)
      .def_readonly("header_size", &ImageInfo::header_size)
      .def_readonly("exports", &ImageInfo::exports)
      .def_readonly("code_relocations", &ImageInfo::code_relocations)
      .def_readonly("imports", &ImageInfo::imports);
  module.def("convert_pic_executable", &ConvertPicExecutable, py::arg("data"),
             py::arg("uid3"), "Convert a restricted, retained-relocation ELF.");
  module.def("inspect_e32", &InspectE32, py::arg("data"),
             "Check the experimental E32 profile, releasing the GIL.");
  module.def(
      "convert_imported_executable", &ConvertImportedExecutable,
      py::arg("data"), py::arg("proxies"), py::arg("uid3"),
      "Convert retained calls through eager ordinal slots, releasing the GIL.");
  module.def(
      "convert_dll", &ConvertDll, py::arg("data"), py::arg("definition"),
      py::arg("proxies"), py::arg("uid3"),
      "Convert frozen DLL exports and eager imports, releasing the GIL.");
  using symbian::sis::PackageOptions;
  py::class_<PackageOptions>(module, "SisPackageOptions")
      .def_readonly("uid", &PackageOptions::uid)
      .def_readonly("name", &PackageOptions::name)
      .def_readonly("vendor", &PackageOptions::vendor)
      .def_readonly("executable_name", &PackageOptions::executable_name)
      .def_readonly("version", &PackageOptions::version);
  using symbian::sis::PackageInfo;
  py::class_<PackageInfo>(module, "SisPackageInfo")
      .def_readonly("options", &PackageInfo::options)
      .def_readonly("executable_uid", &PackageInfo::executable_uid)
      .def_readonly("executable_size", &PackageInfo::executable_size)
      .def_readonly("executable_sha1", &PackageInfo::executable_sha1)
      .def_readonly("target", &PackageInfo::target);
  module.def(
      "build_sis", &BuildSis, py::arg("data"), py::arg("uid"), py::arg("name"),
      py::arg("vendor"), py::arg("executable_name"),
      py::arg("version") = std::array<int32_t, 3>{1, 0, 0},
      "Build the canonical unsigned SISX experiment, releasing the GIL.");
  module.def(
      "inspect_sis", &InspectSis, py::arg("data"),
      "Check the canonical unsigned SISX experiment, releasing the GIL.");
  using symbian::sdk::Export;
  py::class_<Export>(module, "SdkExport")
      .def_readonly("symbol", &Export::symbol)
      .def_readonly("ordinal", &Export::ordinal)
      .def_readonly("data", &Export::data)
      .def_readonly("absent", &Export::absent);
  using symbian::sdk::ProxySources;
  py::class_<ProxySources>(module, "ProxySources")
      .def_readonly("assembly", &ProxySources::assembly)
      .def_readonly("version_script", &ProxySources::version_script)
      .def_readonly("linker_script", &ProxySources::linker_script)
      .def_readonly("exports", &ProxySources::exports);
  using symbian::sdk::ProxyInfo;
  py::class_<ProxyInfo>(module, "ProxyInfo")
      .def_readonly("soname", &ProxyInfo::soname)
      .def_readonly("target_dll", &ProxyInfo::target_dll)
      .def_readonly("exports", &ProxyInfo::exports);
  module.def("parse_def", &ParseDef, py::arg("data"),
             "Parse bounded EABI export declarations, releasing the GIL.");
  module.def("generate_import_proxy", &GenerateProxy, py::arg("data"),
             py::arg("symbols"), py::arg("soname"), py::arg("target_dll"),
             "Generate ordinal proxy sources, releasing the GIL.");
  module.def(
      "inspect_import_proxy", &InspectProxy, py::arg("data"),
      "Check the generated ELF ordinal proxy contract, releasing the GIL.");
}
