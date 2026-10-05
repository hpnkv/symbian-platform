# C++20 on Symbian

Use C++20 with the SDK's Clang ARM toolchain and matching guest runtime.
ARMv5T and ARMv6 use AAPCS soft-float and 16-bit `wchar_t`. Language features
and standard-library facilities have separate requirements: see
[Clang language support](https://clang.llvm.org/cxx_status.html),
[libc++ library support](https://libcxx.llvm.org/Status/Cxx20.html), and the
[guest runtime](runtime.md) for the SDK's available library profiles.

Concepts, constrained lambdas, `consteval`, designated initialization,
structural template arguments, defaulted equality, `char8_t` and
`[[no_unique_address]]` are available. Runtime allocation, global initialization
and concurrency use the SDK implementations rather than host libraries.
Exceptions and RTTI are disabled by default. General C++ TLS, thread-safe
local-static initialization, `import std`, and header units are unsupported.

## A small language example

From a source checkout:

```sh
uv run symbian build --project examples/cxx20_probe \
  --output .symbian/cxx20-probe
uv run symbian package --project examples/cxx20_probe \
  --artifact .symbian/cxx20-probe/cxx20_probe.exe \
  --output .symbian/cxx20-package
uv run symbian inspect --format e32 .symbian/cxx20-probe/cxx20_probe.exe
```

This freestanding example uses no system C/C++ headers, allocation, exceptions
or RTTI. Its startup exits directly and owns no target resources. Use generated
application startup when your program needs the guest runtime or OS handles.

## Named modules

Install upstream Clang with matching `clang-scan-deps`, CMake 3.28+ and
Ninja 1.12+. Put the LLVM tools on `PATH` and build the module example:

```sh
uv run symbian build --project examples/cxx20_module_probe \
  --compiler "$(command -v clang++)" \
  --output .symbian/cxx20-module
uv run symbian package --project examples/cxx20_module_probe \
  --artifact .symbian/cxx20-module/cxx20_module_probe.exe \
  --output .symbian/cxx20-module-package
```

The example uses a `CXX_MODULES` file set and enables scanning for its target.
CMake/Ninja discovers imported BMIs and retains them in the build tree;
ordinary SDK targets leave scanning off. See
[CMake's module rules](https://cmake.org/cmake/help/latest/manual/cmake-cxxmodules.7.html).
Module symbols link within the executable; exporting a module name does not
create an interface to a historical Symbian DLL.

## Standard-library configuration

Use the installed SDK's headers and its selected runtime archive together.
A host libc++ configuration can enable platform services absent on Symbian,
and a header-only overlay cannot supply those services. The SDK generates
its own target configuration following
[LLVM's vendor guidance](https://libcxx.llvm.org/VendorDocumentation.html).

`Symbian::Runtime` supplies the core library; `Symbian::Streams` adds classic
locale and string-stream support. Abseil-based SDK components select the
matching streams profile. Do not link both runtime archives into one image.
For asynchronous code, use the [SDK concurrency APIs](concurrency.md).
