#include <array>
#include <string>

#include <absl/base/nullability.h>
#include <absl/log/initialize.h>
#include <absl/status/status.h>
#include <absl/status/statusor.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11_abseil/status_casters.h>

#include "python/agent_bindings.h"
#include "python/concurrency_interop.h"
#include "python/device_bindings.h"
#include "python/status_interop.h"
#include "symbian/analysis/elf.h"
#include "symbian/e32/e32.h"
#include "symbian/sdk/exports.h"
#include "symbian/sis/sis.h"

namespace py = pybind11;

namespace {

symbian::analysis::Elf32Header InspectElf32(const py::bytes& data) {
  const std::string bytes = data;
  return symbian::python::ValueWithoutGil(
      [&] { return symbian::analysis::InspectElf32(bytes); });
}

py::bytes ConvertPicExecutable(const py::bytes& data, uint32_t uid3,
                               uint32_t capabilities) {
  const std::string bytes = data;
  return py::bytes(symbian::python::ValueWithoutGil([&] {
    return symbian::e32::ConvertPicExecutable(bytes, uid3, capabilities);
  }));
}

py::bytes ConvertEka1Executable(const py::bytes& data, uint32_t uid3,
                                const std::vector<py::bytes>& proxies) {
  const std::string bytes = data;
  std::vector<std::string> libraries;
  for (const auto& proxy : proxies) {
    libraries.emplace_back(proxy);
  }
  return py::bytes(symbian::python::ValueWithoutGil([&] {
    return symbian::e32::ConvertEka1Executable(bytes, uid3, libraries);
  }));
}

symbian::e32::ImageInfo InspectE32(const py::bytes& data) {
  const std::string bytes = data;
  return symbian::python::ValueWithoutGil(
      [&] { return symbian::e32::InspectImage(bytes); });
}

py::bytes ConvertImportedExecutable(const py::bytes& data,
                                    const std::vector<py::bytes>& proxies,
                                    uint32_t uid3, uint32_t capabilities) {
  const std::string bytes = data;
  std::vector<std::string> libraries;
  libraries.reserve(proxies.size());
  for (const auto& proxy : proxies) {
    libraries.emplace_back(proxy);
  }
  return py::bytes(symbian::python::ValueWithoutGil([&] {
    return symbian::e32::ConvertImportedExecutable(bytes, libraries, uid3,
                                                   capabilities);
  }));
}

py::bytes ConvertDll(const py::bytes& data, const py::bytes& definition,
                     const std::vector<py::bytes>& proxies, uint32_t uid3,
                     uint32_t capabilities) {
  const std::string bytes = data;
  const std::string exports = definition;
  std::vector<std::string> libraries;
  libraries.reserve(proxies.size());
  for (const auto& proxy : proxies) {
    libraries.emplace_back(proxy);
  }
  return py::bytes(symbian::python::ValueWithoutGil([&] {
    return symbian::e32::ConvertDll(bytes, exports, libraries, uid3,
                                    capabilities);
  }));
}

py::bytes BuildSis(const py::bytes& data, uint32_t uid, const std::string& name,
                   const std::string& vendor,
                   const std::string& executable_name,
                   const std::array<int32_t, 3>& version) {
  const std::string bytes = data;
  const symbian::sis::PackageOptions options{uid, name, vendor, executable_name,
                                             version};
  return py::bytes(symbian::python::ValueWithoutGil(
      [&] { return symbian::sis::BuildPackage(bytes, options); }));
}

py::bytes BuildRegisteredSis(const py::bytes& data,
                             const py::bytes& registration,
                             const py::bytes& caption, uint32_t uid,
                             const std::string& name, const std::string& vendor,
                             const std::string& executable_name,
                             const std::array<int32_t, 3>& version) {
  const std::string bytes = data;
  const std::string registration_bytes = registration;
  const std::string caption_bytes = caption;
  const symbian::sis::PackageOptions options{uid, name, vendor, executable_name,
                                             version};
  return py::bytes(symbian::python::ValueWithoutGil([&] {
    return symbian::sis::BuildRegisteredPackage(bytes, registration_bytes,
                                                caption_bytes, options);
  }));
}

py::bytes BuildApplicationSis(
    const py::bytes& data,
    const std::vector<std::pair<std::string, py::bytes>>& files, uint32_t uid,
    const std::string& name, const std::string& vendor,
    const std::string& executable_name, const std::array<int32_t, 3>& version,
    const std::vector<std::pair<std::string, py::bytes>>& library_files) {
  const std::string bytes = data;
  std::vector<symbian::sis::ApplicationFile> assets;
  assets.reserve(files.size());
  for (const auto& [target, contents] : files) {
    assets.push_back({target, std::string(contents)});
  }
  std::vector<symbian::sis::ApplicationFile> libraries;
  for (const auto& [target, contents] : library_files) {
    libraries.push_back({target, std::string(contents)});
  }
  const symbian::sis::PackageOptions options{uid, name, vendor, executable_name,
                                             version};
  return py::bytes(symbian::python::ValueWithoutGil([&] {
    return symbian::sis::BuildApplicationPackage(bytes, assets, options,
                                                 libraries);
  }));
}

py::bytes BuildSvgMif(const py::bytes& data) {
  const std::string bytes = data;
  return py::bytes(symbian::python::ValueWithoutGil(
      [&] { return symbian::sis::BuildSvgMif(bytes); }));
}

symbian::sis::PackageInfo InspectSis(const py::bytes& data) {
  const std::string bytes = data;
  return symbian::python::ValueWithoutGil(
      [&] { return symbian::sis::InspectPackage(bytes); });
}

py::bytes SignSis(const py::bytes& data, const py::bytes& certificate,
                  const py::bytes& private_key) {
  const std::string package = data;
  const std::string pem_certificate = certificate;
  const std::string pem_key = private_key;
  return py::bytes(symbian::python::ValueWithoutGil([&] {
    return symbian::sis::SignPackage(package, pem_certificate, pem_key);
  }));
}

std::vector<symbian::sdk::Export> ParseDef(const py::bytes& data) {
  const std::string bytes = data;
  return symbian::python::ValueWithoutGil(
      [&] { return symbian::sdk::ParseExports(bytes); });
}

symbian::sdk::ProxySources GenerateProxy(
    const py::bytes& data, const std::vector<std::string>& symbols,
    const std::string& soname, const std::string& target_dll) {
  const std::string bytes = data;
  return symbian::python::ValueWithoutGil([&] {
    return symbian::sdk::GenerateProxy(bytes, symbols, soname, target_dll);
  });
}

symbian::sdk::ProxyInfo InspectProxy(const py::bytes& data) {
  const std::string bytes = data;
  return symbian::python::ValueWithoutGil(
      [&] { return symbian::sdk::InspectProxy(bytes); });
}

}  // namespace

PYBIND11_MODULE(_native, module) {
  absl::InitializeLog();
  symbian::python::BindAgent(&module);
  symbian::python::InstallPythonSchedulerParkGuard();
  symbian::python::BindConcurrencyInterop(&module);
  symbian::python::BindDevice(&module);
  py::google::ImportStatusModule();
  symbian::python::BindStatus(&module);
  using symbian::analysis::Elf32Header;
  module.doc() = "Stateless native Symbian analysis utilities.";
  py::class_<symbian::analysis::ArmAttributes>(module, "ArmAttributes")
      .def_readonly("cpu_arch", &symbian::analysis::ArmAttributes::cpu_arch)
      .def_readonly("fp_arch", &symbian::analysis::ArmAttributes::fp_arch)
      .def_readonly("simd_arch", &symbian::analysis::ArmAttributes::simd_arch)
      .def_readonly("thumb_isa", &symbian::analysis::ArmAttributes::thumb_isa)
      .def_readonly("vfp_args", &symbian::analysis::ArmAttributes::vfp_args);
  py::class_<Elf32Header>(module, "Elf32Header",
                          "ELF32 metadata; not a loader acceptance result.")
      .def_readonly("type", &Elf32Header::type, "ELF object type.")
      .def_readonly("machine", &Elf32Header::machine, "ELF machine identifier.")
      .def_readonly("entry", &Elf32Header::entry, "ELF entry address.")
      .def_readonly("arm", &Elf32Header::arm)
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
      .def_readonly("ordinal", &ImportSlot::ordinal)
      .def_readonly("addend", &ImportSlot::addend);
  py::class_<ImportBlock>(module, "E32ImportBlock")
      .def_readonly("dll", &ImportBlock::dll)
      .def_readonly("slots", &ImportBlock::slots);
  py::class_<ImageInfo>(module, "E32ImageInfo",
                        "E32 application metadata; no runtime verdict.")
      .def_readonly("kernel", &ImageInfo::kernel)
      .def_readonly("uid3", &ImageInfo::uid3)
      .def_readonly("header_crc", &ImageInfo::header_crc)
      .def_readonly("flags", &ImageInfo::flags)
      .def_readonly("architecture", &ImageInfo::architecture)
      .def_readonly("code_size", &ImageInfo::code_size)
      .def_readonly("code_base", &ImageInfo::code_base)
      .def_readonly("data_size", &ImageInfo::data_size)
      .def_readonly("bss_size", &ImageInfo::bss_size)
      .def_readonly("data_base", &ImageInfo::data_base)
      .def_readonly("entry_offset", &ImageInfo::entry_offset)
      .def_readonly("secure_id", &ImageInfo::secure_id)
      .def_readonly("capabilities", &ImageInfo::capabilities)
      .def_readonly("dll", &ImageInfo::dll)
      .def_readonly("header_size", &ImageInfo::header_size)
      .def_readonly("exception_descriptor_offset",
                    &ImageInfo::exception_descriptor_offset)
      .def_readonly("exports", &ImageInfo::exports)
      .def_readonly("code_relocations", &ImageInfo::code_relocations)
      .def_readonly("code_data_relocations", &ImageInfo::code_data_relocations)
      .def_readonly("data_relocations", &ImageInfo::data_relocations)
      .def_readonly("data_data_relocations", &ImageInfo::data_data_relocations)
      .def_readonly("imports", &ImageInfo::imports);
  module.def("convert_pic_executable", &ConvertPicExecutable, py::arg("data"),
             py::arg("uid3"), py::arg("capabilities") = 0,
             "Convert a restricted, retained-relocation ELF.");
  module.def("convert_eka1_executable", &ConvertEka1Executable, py::arg("data"),
             py::arg("uid3"), py::arg("proxies") = std::vector<py::bytes>{},
             "Convert the EKA1 process and optional EUSER PE imports.");
  module.def("inspect_e32", &InspectE32, py::arg("data"),
             "Check the E32 application profile, releasing the GIL.");
  module.def(
      "convert_imported_executable", &ConvertImportedExecutable,
      py::arg("data"), py::arg("proxies"), py::arg("uid3"),
      py::arg("capabilities") = 0,
      "Convert retained calls through eager ordinal slots, releasing the GIL.");
  module.def(
      "convert_dll", &ConvertDll, py::arg("data"), py::arg("definition"),
      py::arg("proxies"), py::arg("uid3"), py::arg("capabilities") = 0,
      "Convert frozen DLL exports and eager imports, releasing the GIL.");
  using symbian::sis::PackageOptions;
  py::class_<PackageOptions>(module, "SisPackageOptions")
      .def_readonly("uid", &PackageOptions::uid)
      .def_readonly("name", &PackageOptions::name)
      .def_readonly("vendor", &PackageOptions::vendor)
      .def_readonly("executable_name", &PackageOptions::executable_name)
      .def_readonly("version", &PackageOptions::version);
  using symbian::sis::PackageInfo;
  py::class_<PackageInfo::EmbeddedFile>(module, "SisEmbeddedFile")
      .def_readonly("target", &PackageInfo::EmbeddedFile::target)
      .def_readonly("size", &PackageInfo::EmbeddedFile::size)
      .def_readonly("sha1", &PackageInfo::EmbeddedFile::sha1)
      .def_readonly("capabilities", &PackageInfo::EmbeddedFile::capabilities);
  py::class_<PackageInfo>(module, "SisPackageInfo")
      .def_readonly("options", &PackageInfo::options)
      .def_readonly("executable_uid", &PackageInfo::executable_uid)
      .def_readonly("executable_size", &PackageInfo::executable_size)
      .def_readonly("executable_sha1", &PackageInfo::executable_sha1)
      .def_readonly("target", &PackageInfo::target)
      .def_readonly("files", &PackageInfo::files)
      .def_readonly("application_registered",
                    &PackageInfo::application_registered)
      .def_readonly("signed_package", &PackageInfo::signed_package);
  module.def("build_sis", &BuildSis, py::arg("data"), py::arg("uid"),
             py::arg("name"), py::arg("vendor"), py::arg("executable_name"),
             py::arg("version") = std::array<int32_t, 3>{1, 0, 0},
             "Build the canonical unsigned SISX application package, releasing "
             "the GIL.");
  module.def("build_registered_sis", &BuildRegisteredSis, py::arg("data"),
             py::arg("registration"), py::arg("caption"), py::arg("uid"),
             py::arg("name"), py::arg("vendor"), py::arg("executable_name"),
             py::arg("version") = std::array<int32_t, 3>{1, 0, 0},
             "Build a registered unsigned SISX, releasing the GIL.");
  module.def(
      "build_application_sis", &BuildApplicationSis, py::arg("data"),
      py::arg("files"), py::arg("uid"), py::arg("name"), py::arg("vendor"),
      py::arg("executable_name"),
      py::arg("version") = std::array<int32_t, 3>{1, 0, 0},
      py::arg("libraries") = std::vector<std::pair<std::string, py::bytes>>{},
      "Build a localized application SISX, releasing the GIL.");
  module.def("build_svg_mif", &BuildSvgMif, py::arg("data"),
             "Compile a bounded SVG icon into MIF, releasing the GIL.");
  module.def("inspect_sis", &InspectSis, py::arg("data"),
             "Check the canonical SISX application package, releasing "
             "the GIL.");
  module.def("sign_sis", &SignSis, py::arg("data"), py::arg("certificate"),
             py::arg("private_key"),
             "Sign the canonical SISX package, releasing the GIL.");
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
