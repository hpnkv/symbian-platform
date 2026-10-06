#ifndef SYMBIAN_E32_EXPORTS_H_
#define SYMBIAN_E32_EXPORTS_H_

#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <absl/base/nullability.h>
#include <absl/status/statusor.h>

#include "symbian/e32/e32.h"
#include "symbian/e32/imports.h"

namespace symbian::e32::internal {

absl::StatusOr<std::vector<ExportSlot>> ResolveExports(
    std::string_view elf, const std::vector<Section>& sections,
    const Segment& code, uint32_t entry, std::string_view definition);

// Returns a full absence bitmap, retaining one bits beyond the last ordinal.
std::string ExportBitmap(const std::vector<ExportSlot>& exports);

absl::StatusOr<std::string> EncodeCodeRelocations(
    const std::vector<uint32_t>& offsets,
    const std::set<uint32_t>& data_targets = {});
absl::StatusOr<std::vector<uint32_t>> DecodeCodeRelocations(
    std::string_view bytes, uint32_t code_size,
    std::set<uint32_t>* absl_nullable data_targets = nullptr);

}  // namespace symbian::e32::internal

#endif  // SYMBIAN_E32_EXPORTS_H_
