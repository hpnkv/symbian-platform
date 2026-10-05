#ifndef SYMBIAN_SDK_EXPORTS_H_
#define SYMBIAN_SDK_EXPORTS_H_

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <absl/status/statusor.h>

namespace symbian::sdk {

struct Export {
  std::string symbol;
  uint32_t ordinal = 0;
  bool data = false;
  bool absent = false;
};

struct ProxySources {
  std::string assembly;
  std::string version_script;
  std::string linker_script;
  std::vector<Export> exports;
};

struct ProxyInfo {
  std::string soname;
  std::string target_dll;
  std::vector<Export> exports;
};

// Frozen EABI DEF subset: EXPORTS, symbol @ ordinal NONAME, optional DATA size
// and ABSENT. Bounded ASCII input; aliases and other directives are unsupported.
absl::StatusOr<std::vector<Export>> ParseExports(std::string_view text);

// Generates source for Clang/LLD to build an ordinal proxy, NOT target DLL code.
// Empty selection includes every present function/data export. Explicit selections
// must exist and be present. Frozen ordinals are never renumbered.
// Plain names only; decorated DLL version/UID names remain unimplemented.
absl::StatusOr<ProxySources> GenerateProxy(
    std::string_view definition, const std::vector<std::string>& symbols,
    std::string_view soname, std::string_view target_dll);

// Bounded inspection of the generated proxy contract: ARM ELF32, ordinal words
// in section one, file-offset dynamic pointers, symbol/version/name metadata.
// This does not establish a linked executable's E32 or loader compatibility.
absl::StatusOr<ProxyInfo> InspectProxy(std::string_view bytes);

}  // namespace symbian::sdk

#endif  // SYMBIAN_SDK_EXPORTS_H_
