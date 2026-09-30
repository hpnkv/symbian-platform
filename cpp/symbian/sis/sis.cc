#include "symbian/sis/sis.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include <absl/status/status.h>
#include <openssl/sha.h>

#include "symbian/analysis/bytes.h"
#include "symbian/analysis/checksum.h"
#include "symbian/e32/e32.h"

namespace symbian::sis {
namespace {

using analysis::internal::Crc16;
using analysis::internal::Put16;
using analysis::internal::Put32;
using analysis::internal::Read16;
using analysis::internal::Read32;
using analysis::internal::UidChecksum;
using analysis::internal::Within;

constexpr size_t kMaxPayload = 16 * 1024 * 1024;
constexpr size_t kMaxPackage = kMaxPayload + 4096;
constexpr uint32_t kSisUid = 0x10201a7a;

std::string Word(uint32_t value) {
  std::string bytes(4, '\0');
  Put32(bytes, 0, value);
  return bytes;
}

std::string WideWord(uint64_t value) {
  return Word(static_cast<uint32_t>(value)) +
         Word(static_cast<uint32_t>(value >> 32));
}

std::string Half(uint16_t value) {
  std::string bytes(2, '\0');
  Put16(bytes, 0, value);
  return bytes;
}

std::string Field(uint32_t type, std::string_view payload,
                  bool in_array = false) {
  std::string bytes = in_array ? "" : Word(type);
  bytes += Word(static_cast<uint32_t>(payload.size()));
  bytes += payload;
  bytes.append((4 - payload.size() % 4) % 4, '\0');
  return bytes;
}

std::string Array(uint32_t type, std::string_view elements = "") {
  return Field(2, Word(type) + std::string(elements));
}

std::string Utf16(std::string_view ascii) {
  std::string result;
  for (char c : ascii) {
    result += c;
    result += '\0';
  }
  return result;
}

std::string String(std::string_view value) {
  return Field(1, Utf16(value));
}

std::string Strings(std::string_view value) {
  return Array(1, Field(1, Utf16(value), true));
}

std::string Compressed(std::string_view bytes) {
  return Field(3, Word(0) + WideWord(bytes.size()) + std::string(bytes));
}

absl::StatusOr<std::string> Digest(std::string_view bytes) {
  unsigned char digest[SHA_DIGEST_LENGTH];
  if (SHA1(reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size(),
           digest) == nullptr) {
    return absl::InternalError("SHA-1 provider failed");
  }
  return std::string(reinterpret_cast<const char*>(digest), sizeof(digest));
}

absl::Status CheckOptions(const PackageOptions& options) {
  if (options.uid < 0xe0000000 || options.uid > 0xefffffff) {
    return absl::InvalidArgumentError(
        "SIS experiment requires an unprotected experimental UID");
  }
  for (std::string_view text :
       {std::string_view(options.name), std::string_view(options.vendor)}) {
    if (text.empty() || text.size() > 128) {
      return absl::InvalidArgumentError(
          "SIS name/vendor must contain 1..128 printable ASCII characters");
    }
    for (char value : text) {
      const auto c = static_cast<uint8_t>(value);
      if (c < 32 || c > 126) {
        return absl::InvalidArgumentError(
            "SIS metadata requires printable ASCII");
      }
    }
  }
  const std::string& name = options.executable_name;
  if (name.size() < 5 || name.size() > 64 || !name.ends_with(".exe")) {
    return absl::InvalidArgumentError(
        "SIS executable name must end with .exe and contain 5..64 characters");
  }
  for (char c : std::string_view(name).substr(0, name.size() - 4)) {
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '_' || c == '-')) {
      return absl::InvalidArgumentError(
          "SIS executable basename requires ASCII letters, digits, _ or -");
    }
  }
  for (int32_t part : options.version) {
    if (part < 0 || part > 32767) {
      return absl::InvalidArgumentError(
          "SIS version components must be in 0..32767");
    }
  }
  return absl::OkStatus();
}

struct View {
  std::string_view payload;
  std::string_view raw;
};

// Sticky checked cursor: a failure prevents further reads. No allocations are
// driven by on-disk lengths, no recursion, and no decompression is attempted.
class Reader {
 public:
  explicit Reader(std::string_view bytes) : bytes_(bytes) {}

  View Take(uint32_t type, bool in_array = false) {
    if (!status_.ok())
      return {};
    const size_t start = position_;
    if (!in_array && WordValue() != type) {
      Fail(absl::UnimplementedError(
          "SIS field is outside the supported profile"));
      return {};
    }
    const uint32_t length = WordValue();
    if (!status_.ok())
      return {};
    if (length & 0x80000000) {
      Fail(absl::UnimplementedError("Large SIS fields are unsupported"));
      return {};
    }
    const size_t padded = (static_cast<size_t>(length) + 3) & ~size_t{3};
    if (!Within(bytes_.size(), position_, padded)) {
      Fail(absl::DataLossError("SIS field exceeds its container"));
      return {};
    }
    const std::string_view payload = bytes_.substr(position_, length);
    for (size_t i = length; i < padded; ++i) {
      if (bytes_[position_ + i] != '\0') {
        Fail(absl::DataLossError("SIS padding must be zero"));
      }
    }
    position_ += padded;
    return {payload, bytes_.substr(start, position_ - start)};
  }

  uint32_t WordValue() {
    if (!status_.ok())
      return 0;
    if (!Within(bytes_.size(), position_, 4)) {
      Fail(absl::DataLossError("Truncated SIS integer"));
      return 0;
    }
    const uint32_t result = Read32(bytes_, position_);
    position_ += 4;
    return result;
  }

  absl::Status Finish() const {
    if (!status_.ok())
      return status_;
    if (position_ != bytes_.size())
      return absl::UnimplementedError("Extra SIS fields are unsupported");
    return absl::OkStatus();
  }

  const absl::Status& status() const { return status_; }

 private:
  void Fail(absl::Status status) {
    if (status_.ok())
      status_ = std::move(status);
  }

  std::string_view bytes_;
  size_t position_ = 0;
  absl::Status status_;
};

absl::StatusOr<std::string_view> Single(std::string_view bytes, uint32_t type) {
  Reader array(bytes);
  if (array.WordValue() != type)
    return absl::UnimplementedError("Unsupported SIS array element type");
  const View element = array.Take(type, true);
  if (const auto status = array.Finish(); !status.ok())
    return status;
  return element.payload;
}

absl::StatusOr<std::string> Ascii(std::string_view bytes) {
  if (bytes.size() % 2)
    return absl::DataLossError("Odd SIS UTF-16 string length");
  if (bytes.size() > 256)
    return absl::UnimplementedError("SIS string exceeds profile limit");
  std::string text;
  for (size_t i = 0; i < bytes.size(); i += 2) {
    const uint16_t c = Read16(bytes, i);
    if (c < 32 || c > 126)
      return absl::UnimplementedError("Non-ASCII SIS strings are unsupported");
    text += static_cast<char>(c);
  }
  return text;
}

absl::StatusOr<std::string_view> Uncompressed(std::string_view bytes) {
  if (bytes.size() < 12)
    return absl::DataLossError("Truncated SIS compressed field");
  if (Read32(bytes, 0) != 0)
    return absl::UnimplementedError("Compressed SIS streams are unsupported");
  if (Read32(bytes, 8) != 0 || Read32(bytes, 4) != bytes.size() - 12) {
    return absl::DataLossError("SIS uncompressed length mismatch");
  }
  return bytes.substr(12);
}

absl::Status CheckCrc(View checksum, View field) {
  if (checksum.payload.size() != 2)
    return absl::DataLossError("Invalid SIS checksum length");
  if (Read16(checksum.payload, 0) != Crc16(field.raw))
    return absl::DataLossError("SIS CRC16 mismatch");
  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<std::string> BuildPackage(std::string_view executable,
                                         const PackageOptions& options) {
  if (const auto status = CheckOptions(options); !status.ok())
    return status;
  if (executable.size() > kMaxPayload)
    return absl::ResourceExhaustedError(
        "SIS experimental payload exceeds 16 MiB");
  const auto image = e32::InspectImage(executable);
  if (!image.ok())
    return image.status();
  const auto digest = Digest(executable);
  if (!digest.ok())
    return digest.status();
  std::string version;
  for (int32_t part : options.version)
    version += Word(static_cast<uint32_t>(part));
  // Fixed historical epoch: package output must not depend on wall-clock time.
  const std::string date = Half(2004) + std::string("\0\1", 2);
  const std::string datetime =
      Field(8, Field(6, date) + Field(7, std::string(3, '\0')));
  const std::string info =
      Field(14, Field(9, Word(options.uid)) + String(options.vendor) +
                    Strings(options.name) + Strings(options.vendor) +
                    Field(4, version) + datetime + std::string(2, '\0'));
  const std::string target = "!:\\sys\\bin\\" + options.executable_name;
  const std::string file = String(target) + String("") +
                           Field(25, Word(1) + Field(37, *digest)) + Word(1) +
                           Word(0) + WideWord(executable.size()) +
                           WideWord(executable.size()) + Word(0);
  const std::string block =
      Field(28, Array(24, Field(24, file, true)) + Array(13) + Array(26));
  const std::string controller =
      Field(13, info + Field(16, Array(33)) +
                    Field(15, Array(11, Field(11, Word(1), true))) +
                    Field(17, Array(18) + Array(18)) + Field(19, Array(20)) +
                    block + Field(40, Word(0)));
  const std::string compressed = Compressed(controller);
  const std::string data = Field(
      30,
      Array(31, Field(31, Array(32, Field(32, Compressed(executable), true)),
                      true)));
  std::string header = Word(kSisUid) + Word(0) + Word(options.uid) + Word(0);
  Put32(header, 12, UidChecksum(header));
  return header +
         Field(12, Field(34, Half(Crc16(compressed))) +
                       Field(35, Half(Crc16(data))) + compressed + data);
}

absl::StatusOr<PackageInfo> InspectPackage(std::string_view bytes) {
  if (bytes.size() > kMaxPackage)
    return absl::ResourceExhaustedError("SIS experiment exceeds size limit");
  if (bytes.size() < 16)
    return absl::DataLossError("Truncated SIS UID header");
  if (Read32(bytes, 0) != kSisUid || Read32(bytes, 4) != 0)
    return absl::UnimplementedError("Requires the supported SISX UID profile");
  if (Read32(bytes, 12) != UidChecksum(bytes))
    return absl::DataLossError("SIS UID checksum mismatch");
  Reader root(bytes.substr(16));
  Reader contents(root.Take(12).payload);
  if (const auto status = root.Finish(); !status.ok())
    return status;
  const View controller_crc = contents.Take(34);
  const View data_crc = contents.Take(35);
  const View compressed = contents.Take(3);
  const View data = contents.Take(30);
  if (const auto status = contents.Finish(); !status.ok())
    return status;
  if (const auto status = CheckCrc(controller_crc, compressed); !status.ok())
    return status;
  if (const auto status = CheckCrc(data_crc, data); !status.ok())
    return status;
  const auto controller_bytes = Uncompressed(compressed.payload);
  if (!controller_bytes.ok())
    return controller_bytes.status();
  Reader controller_root(*controller_bytes);
  Reader controller(controller_root.Take(13).payload);
  if (const auto status = controller_root.Finish(); !status.ok())
    return status;
  Reader info(controller.Take(14).payload);
  Reader uid(info.Take(9).payload);
  PackageInfo result;
  result.options.uid = uid.WordValue();
  if (const auto status = uid.Finish(); !status.ok())
    return status;
  const auto vendor = Ascii(info.Take(1).payload);
  if (!vendor.ok())
    return vendor.status();
  result.options.vendor = *vendor;
  const auto name_element = Single(info.Take(2).payload, 1);
  if (!name_element.ok())
    return name_element.status();
  const auto name = Ascii(*name_element);
  if (!name.ok())
    return name.status();
  result.options.name = *name;
  info.Take(2);  // Localized vendor; canonical comparison verifies it.
  Reader version(info.Take(4).payload);
  for (int32_t& part : result.options.version) {
    const uint32_t value = version.WordValue();
    if (value > 32767)
      return absl::UnimplementedError("SIS version exceeds profile limit");
    part = static_cast<int32_t>(value);
  }
  if (const auto status = version.Finish(); !status.ok())
    return status;
  if (!info.status().ok())
    return info.status();
  controller.Take(16);
  controller.Take(15);
  controller.Take(17);
  controller.Take(19);
  Reader block(controller.Take(28).payload);
  const auto file_element = Single(block.Take(2).payload, 24);
  if (!file_element.ok())
    return file_element.status();
  Reader file(*file_element);
  const auto target = Ascii(file.Take(1).payload);
  if (!target.ok())
    return target.status();
  result.target = *target;
  constexpr std::string_view prefix = "!:\\sys\\bin\\";
  if (!result.target.starts_with(prefix))
    return absl::UnimplementedError("Unsupported SIS install destination");
  result.options.executable_name = result.target.substr(prefix.size());
  file.Take(1);
  Reader hash(file.Take(25).payload);
  if (hash.WordValue() != 1)
    return absl::UnimplementedError("Unsupported SIS hash algorithm");
  const View expected_digest = hash.Take(37);
  if (const auto status = hash.Finish(); !status.ok())
    return status;
  if (!file.status().ok())
    return file.status();
  if (!block.status().ok())
    return block.status();
  if (!controller.status().ok())
    return controller.status();
  Reader data_reader(data.payload);
  const auto unit = Single(data_reader.Take(2).payload, 31);
  if (!unit.ok())
    return unit.status();
  if (const auto status = data_reader.Finish(); !status.ok())
    return status;
  Reader unit_reader(*unit);
  const auto file_data = Single(unit_reader.Take(2).payload, 32);
  if (!file_data.ok())
    return file_data.status();
  if (const auto status = unit_reader.Finish(); !status.ok())
    return status;
  Reader payload_reader(*file_data);
  const auto payload = Uncompressed(payload_reader.Take(3).payload);
  if (!payload.ok())
    return payload.status();
  if (const auto status = payload_reader.Finish(); !status.ok())
    return status;
  const auto digest = Digest(*payload);
  if (!digest.ok())
    return digest.status();
  if (*digest != expected_digest.payload)
    return absl::DataLossError("SIS executable SHA-1 mismatch");
  const auto canonical = BuildPackage(*payload, result.options);
  if (!canonical.ok())
    return canonical.status();
  if (*canonical != bytes)
    return absl::UnimplementedError(
        "SIS fields are outside the canonical unsigned experiment profile");
  result.executable_uid =
      Read32(*payload, 8);  // Native E32 inspection succeeded.
  result.executable_size = static_cast<uint32_t>(payload->size());
  return result;
}

}  // namespace symbian::sis
