#include "symbian/sis/sis.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <absl/status/status.h>
#include <openssl/sha.h>
#include <zlib.h>

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
constexpr size_t kMaxPackage = kMaxPayload + 64 * 1024;
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
        "SIS application package requires an unprotected experimental UID");
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
    if (!status_.ok()) {
      return {};
    }
    const size_t start = position_;
    if (!in_array && WordValue() != type) {
      Fail(absl::UnimplementedError(
          "SIS field is outside the supported profile"));
      return {};
    }
    const uint32_t length = WordValue();
    if (!status_.ok()) {
      return {};
    }
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
    if (!status_.ok()) {
      return 0;
    }
    if (!Within(bytes_.size(), position_, 4)) {
      Fail(absl::DataLossError("Truncated SIS integer"));
      return 0;
    }
    const uint32_t result = Read32(bytes_, position_);
    position_ += 4;
    return result;
  }

  absl::Status Finish() const {
    if (!status_.ok()) {
      return status_;
    }
    if (position_ != bytes_.size()) {
      return absl::UnimplementedError("Extra SIS fields are unsupported");
    }
    return absl::OkStatus();
  }

  const absl::Status& status() const { return status_; }

  bool Done() const { return position_ == bytes_.size(); }

 private:
  void Fail(absl::Status status) {
    if (status_.ok()) {
      status_ = std::move(status);
    }
  }

  std::string_view bytes_;
  size_t position_ = 0;
  absl::Status status_;
};

absl::StatusOr<std::string_view> Single(std::string_view bytes, uint32_t type) {
  Reader array(bytes);
  if (array.WordValue() != type) {
    return absl::UnimplementedError("Unsupported SIS array element type");
  }
  const View element = array.Take(type, true);
  if (const auto status = array.Finish(); !status.ok()) {
    return status;
  }
  return element.payload;
}

absl::StatusOr<std::vector<std::string_view>> Elements(std::string_view bytes,
                                                       uint32_t type,
                                                       size_t maximum) {
  Reader array(bytes);
  if (array.WordValue() != type) {
    return absl::UnimplementedError("Unsupported SIS array element type");
  }
  std::vector<std::string_view> result;
  while (!array.Done() && array.status().ok()) {
    if (result.size() == maximum) {
      return absl::UnimplementedError("Too many SIS array elements");
    }
    result.push_back(array.Take(type, true).payload);
  }
  if (const auto status = array.Finish(); !status.ok()) {
    return status;
  }
  return result;
}

absl::StatusOr<std::string> Ascii(std::string_view bytes) {
  if (bytes.size() % 2) {
    return absl::DataLossError("Odd SIS UTF-16 string length");
  }
  if (bytes.size() > 256) {
    return absl::UnimplementedError("SIS string exceeds profile limit");
  }
  std::string text;
  for (size_t i = 0; i < bytes.size(); i += 2) {
    const uint16_t c = Read16(bytes, i);
    if (c < 32 || c > 126) {
      return absl::UnimplementedError("Non-ASCII SIS strings are unsupported");
    }
    text += static_cast<char>(c);
  }
  return text;
}

absl::StatusOr<std::string_view> Uncompressed(std::string_view bytes) {
  if (bytes.size() < 12) {
    return absl::DataLossError("Truncated SIS compressed field");
  }
  if (Read32(bytes, 0) != 0) {
    return absl::UnimplementedError("Compressed SIS streams are unsupported");
  }
  if (Read32(bytes, 8) != 0 || Read32(bytes, 4) != bytes.size() - 12) {
    return absl::DataLossError("SIS uncompressed length mismatch");
  }
  return bytes.substr(12);
}

absl::Status CheckCrc(View checksum, View field) {
  if (checksum.payload.size() != 2) {
    return absl::DataLossError("Invalid SIS checksum length");
  }
  if (Read16(checksum.payload, 0) != Crc16(field.raw)) {
    return absl::DataLossError("SIS CRC16 mismatch");
  }
  return absl::OkStatus();
}

struct SourceFile {
  std::string target;
  std::string_view bytes;
};

absl::Status CheckResource(std::string_view bytes, uint32_t uid2,
                           uint32_t uid3) {
  if (bytes.size() < 24 || bytes.size() > 32768) {
    return absl::InvalidArgumentError("Registration resource size is invalid");
  }
  if (Read32(bytes, 0) != 0x101f4a6b || Read32(bytes, 4) != uid2 ||
      Read32(bytes, 8) != uid3 || Read32(bytes, 12) != UidChecksum(bytes)) {
    return absl::InvalidArgumentError("Registration resource UID mismatch");
  }
  return absl::OkStatus();
}

absl::StatusOr<std::string> BuildFiles(
    std::string_view executable, const PackageOptions& options,
    const std::vector<ApplicationFile>& assets) {
  if (const auto status = CheckOptions(options); !status.ok()) {
    return status;
  }
  if (executable.size() > kMaxPayload) {
    return absl::ResourceExhaustedError("SIS payload exceeds 16 MiB");
  }
  const auto image = e32::InspectImage(executable);
  if (!image.ok()) {
    return image.status();
  }
  if (image->dll) {
    return absl::UnimplementedError("SIS requires an executable, not a DLL");
  }
  if (!assets.empty() && (assets.size() < 2 || assets.size() > 40)) {
    return absl::InvalidArgumentError("Expected 2..40 application assets");
  }
  const bool registered = !assets.empty();
  if (registered) {
    const std::string stem =
        options.executable_name.substr(0, options.executable_name.size() - 4);
    const std::string registration_target =
        "!:\\private\\10003a3f\\import\\apps\\" + stem + "_reg.rsc";
    const std::string local_target = "!:\\resource\\apps\\" + stem + "_loc";
    if (assets[0].target != registration_target ||
        assets[1].target != local_target + ".rsc") {
      return absl::InvalidArgumentError(
          "Application resource targets mismatch");
    }
    if (const auto status =
            CheckResource(assets[0].bytes, 0x101f8021, image->uid3);
        !status.ok()) {
      return status;
    }
    std::string previous;
    bool icon_seen = false;
    bool ca_seen = false;
    for (size_t index = 1; index < assets.size(); ++index) {
      const auto& asset = assets[index];
      if (index == 1) {
        if (const auto status = CheckResource(asset.bytes, 0, 0);
            !status.ok()) {
          return status;
        }
        continue;
      }
      if (asset.target == "!:\\resource\\apps\\" + stem + ".mif") {
        if (icon_seen || index + 1 != assets.size() ||
            asset.bytes.size() < 66 || Read32(asset.bytes, 0) != 0x34232342 ||
            Read32(asset.bytes, 4) != 2 || Read32(asset.bytes, 8) != 16 ||
            Read32(asset.bytes, 12) != 2 || Read32(asset.bytes, 16) != 32 ||
            Read32(asset.bytes, 20) != asset.bytes.size() - 32 ||
            Read32(asset.bytes, 24) != 32 ||
            Read32(asset.bytes, 28) != asset.bytes.size() - 32 ||
            Read32(asset.bytes, 32) != 0x34232343 ||
            Read32(asset.bytes, 36) != 1 || Read32(asset.bytes, 40) != 32 ||
            Read32(asset.bytes, 44) != asset.bytes.size() - 64 ||
            Read32(asset.bytes, 48) != 1 ||
            static_cast<uint8_t>(asset.bytes[64]) != 0x1f ||
            static_cast<uint8_t>(asset.bytes[65]) != 0x8b) {
          return absl::InvalidArgumentError("Invalid application MIF icon");
        }
        icon_seen = true;
        continue;
      }
      if (asset.target == "!:\\resource\\apps\\" + stem + "_ca.pem") {
        if (ca_seen || icon_seen || asset.bytes.empty() ||
            asset.bytes.size() > 262144 ||
            !asset.bytes.starts_with("-----BEGIN CERTIFICATE-----") ||
            asset.bytes.find("-----END CERTIFICATE-----") ==
                std::string::npos) {
          return absl::InvalidArgumentError("Invalid project CA bundle");
        }
        ca_seen = true;
        continue;
      }
      if (ca_seen) {
        return absl::InvalidArgumentError("Locale follows CA bundle");
      }
      if (!asset.target.starts_with(local_target + ".r") ||
          asset.target.size() != local_target.size() + 4 ||
          asset.target[asset.target.size() - 2] < '0' ||
          asset.target[asset.target.size() - 2] > '9' ||
          asset.target.back() < '0' || asset.target.back() > '9' ||
          (!previous.empty() && asset.target <= previous)) {
        return absl::InvalidArgumentError(
            "Invalid locale resource target/order");
      }
      if (const auto status = CheckResource(asset.bytes, 0, 0); !status.ok()) {
        return status;
      }
      previous = asset.target;
    }
  }
  std::vector<SourceFile> files = {
      {"!:\\sys\\bin\\" + options.executable_name, executable}};
  for (const auto& asset : assets) {
    files.push_back({asset.target, asset.bytes});
  }
  size_t total = 0;
  for (const auto& file : files) {
    total += file.bytes.size();
  }
  if (total > kMaxPayload) {
    return absl::ResourceExhaustedError("SIS payload exceeds 16 MiB");
  }
  std::string version;
  for (int32_t part : options.version) {
    version += Word(static_cast<uint32_t>(part));
  }
  const std::string date = Half(2004) + std::string("\0\1", 2);
  const std::string datetime =
      Field(8, Field(6, date) + Field(7, std::string(3, '\0')));
  const std::string info =
      Field(14, Field(9, Word(options.uid)) + String(options.vendor) +
                    Strings(options.name) + Strings(options.vendor) +
                    Field(4, version) + datetime + std::string(2, '\0'));
  std::string descriptions;
  std::string payloads;
  for (size_t index = 0; index < files.size(); ++index) {
    const auto digest = Digest(files[index].bytes);
    if (!digest.ok()) {
      return digest.status();
    }
    const std::string file = String(files[index].target) + String("") +
                             Field(25, Word(1) + Field(37, *digest)) + Word(1) +
                             Word(0) + WideWord(files[index].bytes.size()) +
                             WideWord(files[index].bytes.size()) +
                             Word(static_cast<uint32_t>(index));
    descriptions += Field(24, file, true);
    payloads += Field(32, Compressed(files[index].bytes), true);
  }
  const std::string block =
      Field(28, Array(24, descriptions) + Array(13) + Array(26));
  const std::string controller =
      Field(13, info + Field(16, Array(33)) +
                    Field(15, Array(11, Field(11, Word(1), true))) +
                    Field(17, Array(18) + Array(18)) + Field(19, Array(20)) +
                    block + Field(40, Word(0)));
  const std::string compressed = Compressed(controller);
  const std::string data =
      Field(30, Array(31, Field(31, Array(32, payloads), true)));
  std::string header = Word(kSisUid) + Word(0) + Word(options.uid) + Word(0);
  Put32(header, 12, UidChecksum(header));
  return header +
         Field(12, Field(34, Half(Crc16(compressed))) +
                       Field(35, Half(Crc16(data))) + compressed + data);
}

}  // namespace

absl::StatusOr<std::string> BuildPackage(std::string_view executable,
                                         const PackageOptions& options) {
  return BuildFiles(executable, options, {});
}

absl::StatusOr<std::string> BuildRegisteredPackage(
    std::string_view executable, std::string_view registration,
    std::string_view caption, const PackageOptions& options) {
  const std::string stem =
      options.executable_name.substr(0, options.executable_name.size() - 4);
  return BuildFiles(
      executable, options,
      {{"!:\\private\\10003a3f\\import\\apps\\" + stem + "_reg.rsc",
        std::string(registration)},
       {"!:\\resource\\apps\\" + stem + "_loc.rsc", std::string(caption)}});
}

absl::StatusOr<std::string> BuildApplicationPackage(
    std::string_view executable, const std::vector<ApplicationFile>& assets,
    const PackageOptions& options) {
  return BuildFiles(executable, options, assets);
}

absl::StatusOr<std::string> BuildSvgMif(std::string_view svg) {
  if (svg.empty() || svg.size() > 1024 * 1024 ||
      svg.find("<svg") == std::string_view::npos) {
    return absl::InvalidArgumentError("Expected an SVG asset <= 1 MiB");
  }
  z_stream stream{};
  if (deflateInit2(&stream, Z_BEST_COMPRESSION, Z_DEFLATED, MAX_WBITS + 16, 8,
                   Z_DEFAULT_STRATEGY) != Z_OK) {
    return absl::InternalError("SVG gzip encoder initialization failed");
  }
  gz_header header{};
  header.time = 0;
  if (deflateSetHeader(&stream, &header) != Z_OK) {
    deflateEnd(&stream);
    return absl::InternalError("SVG gzip header setup failed");
  }
  std::string gzip(compressBound(svg.size()) + 32, '\0');
  stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(svg.data()));
  stream.avail_in = static_cast<uInt>(svg.size());
  stream.next_out = reinterpret_cast<Bytef*>(gzip.data());
  stream.avail_out = static_cast<uInt>(gzip.size());
  const int result = deflate(&stream, Z_FINISH);
  deflateEnd(&stream);
  if (result != Z_STREAM_END) {
    return absl::InternalError("SVG gzip encoding failed");
  }
  gzip.resize(stream.total_out);
  const uint32_t entry_size = static_cast<uint32_t>(32 + gzip.size());
  std::string mif = Word(0x34232342) + Word(2) + Word(16) + Word(2);
  mif += Word(32) + Word(entry_size);
  mif += Word(32) + Word(entry_size);
  mif += Word(0x34232343) + Word(1) + Word(32) +
         Word(static_cast<uint32_t>(gzip.size())) + Word(1) + Word(7) +
         Word(0) + Word(4) + gzip;
  return mif;
}

absl::StatusOr<PackageInfo> InspectPackage(std::string_view bytes) {
  if (bytes.size() > kMaxPackage) {
    return absl::ResourceExhaustedError(
        "SIS application package exceeds size limit");
  }
  if (bytes.size() < 16) {
    return absl::DataLossError("Truncated SIS UID header");
  }
  if (Read32(bytes, 0) != kSisUid || Read32(bytes, 4) != 0) {
    return absl::UnimplementedError("Requires the supported SISX UID profile");
  }
  if (Read32(bytes, 12) != UidChecksum(bytes)) {
    return absl::DataLossError("SIS UID checksum mismatch");
  }
  Reader root(bytes.substr(16));
  Reader contents(root.Take(12).payload);
  if (const auto status = root.Finish(); !status.ok()) {
    return status;
  }
  const View controller_crc = contents.Take(34);
  const View data_crc = contents.Take(35);
  const View compressed = contents.Take(3);
  const View data = contents.Take(30);
  if (const auto status = contents.Finish(); !status.ok()) {
    return status;
  }
  if (const auto status = CheckCrc(controller_crc, compressed); !status.ok()) {
    return status;
  }
  if (const auto status = CheckCrc(data_crc, data); !status.ok()) {
    return status;
  }
  const auto controller_bytes = Uncompressed(compressed.payload);
  if (!controller_bytes.ok()) {
    return controller_bytes.status();
  }
  Reader controller_root(*controller_bytes);
  Reader controller(controller_root.Take(13).payload);
  if (const auto status = controller_root.Finish(); !status.ok()) {
    return status;
  }
  Reader info(controller.Take(14).payload);
  Reader uid(info.Take(9).payload);
  PackageInfo result;
  result.options.uid = uid.WordValue();
  if (const auto status = uid.Finish(); !status.ok()) {
    return status;
  }
  const auto vendor = Ascii(info.Take(1).payload);
  if (!vendor.ok()) {
    return vendor.status();
  }
  result.options.vendor = *vendor;
  const auto name_element = Single(info.Take(2).payload, 1);
  if (!name_element.ok()) {
    return name_element.status();
  }
  const auto name = Ascii(*name_element);
  if (!name.ok()) {
    return name.status();
  }
  result.options.name = *name;
  info.Take(2);  // Localized vendor; canonical comparison verifies it.
  Reader version(info.Take(4).payload);
  for (int32_t& part : result.options.version) {
    const uint32_t value = version.WordValue();
    if (value > 32767) {
      return absl::UnimplementedError("SIS version exceeds profile limit");
    }
    part = static_cast<int32_t>(value);
  }
  if (const auto status = version.Finish(); !status.ok()) {
    return status;
  }
  if (!info.status().ok()) {
    return info.status();
  }
  controller.Take(16);
  controller.Take(15);
  controller.Take(17);
  controller.Take(19);
  Reader block(controller.Take(28).payload);
  const auto descriptions = Elements(block.Take(2).payload, 24, 41);
  if (!descriptions.ok()) {
    return descriptions.status();
  }
  if (descriptions->size() != 1 &&
      (descriptions->size() < 3 || descriptions->size() > 41)) {
    return absl::UnimplementedError("Unsupported SIS file count");
  }
  constexpr std::string_view prefix = "!:\\sys\\bin\\";
  std::vector<std::string> expected_digests;
  for (const auto description : *descriptions) {
    Reader file(description);
    const auto target = Ascii(file.Take(1).payload);
    if (!target.ok()) {
      return target.status();
    }
    result.files.push_back({*target, 0, {}});
    file.Take(1);
    Reader hash(file.Take(25).payload);
    if (hash.WordValue() != 1) {
      return absl::UnimplementedError("Unsupported SIS hash algorithm");
    }
    const View digest = hash.Take(37);
    if (const auto status = hash.Finish(); !status.ok()) {
      return status;
    }
    expected_digests.emplace_back(digest.payload);
    if (!file.status().ok()) {
      return file.status();
    }
  }
  result.target = result.files.front().target;
  if (!result.target.starts_with(prefix)) {
    return absl::UnimplementedError("Unsupported SIS install destination");
  }
  result.options.executable_name = result.target.substr(prefix.size());
  if (!block.status().ok()) {
    return block.status();
  }
  if (!controller.status().ok()) {
    return controller.status();
  }
  Reader data_reader(data.payload);
  const auto unit = Single(data_reader.Take(2).payload, 31);
  if (!unit.ok()) {
    return unit.status();
  }
  if (const auto status = data_reader.Finish(); !status.ok()) {
    return status;
  }
  Reader unit_reader(*unit);
  const auto file_data = Elements(unit_reader.Take(2).payload, 32, 41);
  if (!file_data.ok()) {
    return file_data.status();
  }
  if (const auto status = unit_reader.Finish(); !status.ok()) {
    return status;
  }
  if (file_data->size() != result.files.size()) {
    return absl::DataLossError("SIS file description/data count mismatch");
  }
  std::vector<std::string_view> payloads;
  constexpr char kHex[] = "0123456789abcdef";
  for (size_t index = 0; index < file_data->size(); ++index) {
    Reader payload_reader((*file_data)[index]);
    const auto payload = Uncompressed(payload_reader.Take(3).payload);
    if (!payload.ok()) {
      return payload.status();
    }
    if (const auto status = payload_reader.Finish(); !status.ok()) {
      return status;
    }
    const auto digest = Digest(*payload);
    if (!digest.ok()) {
      return digest.status();
    }
    if (*digest != expected_digests[index]) {
      return absl::DataLossError("SIS file SHA-1 mismatch");
    }
    payloads.push_back(*payload);
    result.files[index].size = static_cast<uint32_t>(payload->size());
    for (const char value : *digest) {
      const auto byte = static_cast<unsigned char>(value);
      result.files[index].sha1.push_back(kHex[byte >> 4]);
      result.files[index].sha1.push_back(kHex[byte & 15]);
    }
  }
  std::vector<ApplicationFile> assets;
  for (size_t index = 1; index < payloads.size(); ++index) {
    assets.push_back(
        {result.files[index].target, std::string(payloads[index])});
  }
  const auto canonical =
      payloads.size() == 1
          ? BuildPackage(payloads[0], result.options)
          : BuildApplicationPackage(payloads[0], assets, result.options);
  if (!canonical.ok()) {
    return canonical.status();
  }
  if (*canonical != bytes) {
    return absl::UnimplementedError(
        "SIS fields are outside the canonical unsigned application package "
        "profile");
  }
  result.executable_uid = Read32(payloads[0], 8);
  result.executable_size = result.files[0].size;
  result.executable_sha1 = result.files[0].sha1;
  result.application_registered = payloads.size() >= 3;
  return result;
}

}  // namespace symbian::sis
