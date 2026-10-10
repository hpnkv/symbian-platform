#include "symbian/sis/sis.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <absl/base/nullability.h>
#include <absl/status/status.h>
#include <absl/status/status_macros.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/sha.h>
#include <openssl/x509.h>
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
  for (const char c : ascii) {
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
  for (const std::string_view text :
       {std::string_view(options.name), std::string_view(options.vendor)}) {
    if (text.empty() || text.size() > 128) {
      return absl::InvalidArgumentError(
          "SIS name/vendor must contain 1..128 printable ASCII characters");
    }
    for (const char value : text) {
      if (const auto c = static_cast<uint8_t>(value); c < 32 || c > 126) {
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
  for (const char c : std::string_view(name).substr(0, name.size() - 4)) {
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '_' || c == '-')) {
      return absl::InvalidArgumentError(
          "SIS executable basename requires ASCII letters, digits, _ or -");
    }
  }
  for (const int32_t part : options.version) {
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
    return {.payload = payload, .raw = bytes_.substr(start, position_ - start)};
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
    ABSL_RETURN_IF_ERROR(status_);
    if (position_ != bytes_.size()) {
      return absl::UnimplementedError("Extra SIS fields are unsupported");
    }
    return absl::OkStatus();
  }

  const absl::Status& status() const { return status_; }

  bool Done() const { return position_ == bytes_.size(); }

  uint32_t PeekType() const {
    return Within(bytes_.size(), position_, 4) ? Read32(bytes_, position_) : 0;
  }

  size_t position() const { return position_; }

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
  ABSL_RETURN_IF_ERROR(array.Finish());
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
  ABSL_RETURN_IF_ERROR(array.Finish());
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
    const std::vector<ApplicationFile>& assets,
    const std::vector<ApplicationFile>& libraries = {}) {
  ABSL_RETURN_IF_ERROR(CheckOptions(options));
  if (executable.size() > kMaxPayload) {
    return absl::ResourceExhaustedError("SIS payload exceeds 16 MiB");
  }
  ABSL_ASSIGN_OR_RETURN(const auto image, e32::InspectImage(executable));
  if (image.kernel != "eka2") {
    return absl::UnimplementedError("EKA1 requires legacy SIS, not SISX");
  }
  if (image.dll) {
    return absl::UnimplementedError("SIS requires an executable, not a DLL");
  }
  if (!assets.empty() && (assets.size() < 2 || assets.size() > 40)) {
    return absl::InvalidArgumentError("Expected 2..40 application assets");
  }
  if (const bool registered = !assets.empty(); registered) {
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
    ABSL_RETURN_IF_ERROR(
        CheckResource(assets[0].bytes, 0x101f8021, image.uid3));
    std::string previous;
    bool icon_seen = false;
    bool ca_seen = false;
    for (size_t index = 1; index < assets.size(); ++index) {
      const auto& asset = assets[index];
      if (index == 1) {
        ABSL_RETURN_IF_ERROR(CheckResource(asset.bytes, 0, 0));
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
      ABSL_RETURN_IF_ERROR(CheckResource(asset.bytes, 0, 0));
      previous = asset.target;
    }
  }
  std::vector<SourceFile> files = {
      {"!:\\sys\\bin\\" + options.executable_name, executable}};
  for (const auto& asset : assets) {
    files.push_back({asset.target, asset.bytes});
  }
  if (assets.size() + libraries.size() > 40) {
    return absl::ResourceExhaustedError(
        "SIS supports at most 40 additional files");
  }
  std::string previous_library;
  for (const auto& library : libraries) {
    constexpr std::string_view prefix = "!:\\sys\\bin\\";
    const std::string_view target = library.target;
    std::string library_key(library.target);
    for (char& c : library_key) {
      if (c >= 'A' && c <= 'Z') {
        c = static_cast<char>(c + ('a' - 'A'));
      }
    }
    if (!target.starts_with(prefix) || !target.ends_with(".dll") ||
        target.size() <= prefix.size() + 4 || target.size() > 128 ||
        (!previous_library.empty() && library_key <= previous_library)) {
      return absl::InvalidArgumentError(
          "DLL install targets must be unique, sorted !:\\sys\\bin\\ names");
    }
    const auto filename =
        target.substr(prefix.size(), target.size() - prefix.size() - 4);
    for (const char c : filename) {
      if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_' || c == '-')) {
        return absl::InvalidArgumentError(
            "DLL filename requires ASCII letters, digits, hyphens or "
            "underscores");
      }
    }
    ABSL_ASSIGN_OR_RETURN(const auto dll, e32::InspectImage(library.bytes));
    if (!dll.dll || dll.kernel != image.kernel ||
        dll.architecture != image.architecture) {
      return absl::InvalidArgumentError(
          "Packaged DLL must match the application's architecture and kernel");
    }
    if ((dll.capabilities & image.capabilities) != image.capabilities) {
      return absl::InvalidArgumentError(
          "Packaged DLL capabilities do not cover the application");
    }
    files.push_back({library.target, library.bytes});
    previous_library = library_key;
  }
  size_t total = 0;
  for (const auto& file : files) {
    total += file.bytes.size();
  }
  if (total > kMaxPayload) {
    return absl::ResourceExhaustedError("SIS payload exceeds 16 MiB");
  }
  std::string version;
  for (const int32_t part : options.version) {
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
    ABSL_ASSIGN_OR_RETURN(const auto digest, Digest(files[index].bytes));
    uint32_t capability_bits = index == 0 ? image.capabilities : 0;
    if (index > assets.size()) {
      ABSL_ASSIGN_OR_RETURN(const auto dll,
                            e32::InspectImage(files[index].bytes));
      capability_bits = dll.capabilities;
    }
    const std::string capabilities =
        capability_bits != 0 ? Field(41, Word(capability_bits)) : std::string();
    const std::string file = String(files[index].target) + String("") +
                             capabilities +
                             Field(25, Word(1) + Field(37, digest)) + Word(1) +
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
    const PackageOptions& options,
    const std::vector<ApplicationFile>& libraries) {
  return BuildFiles(executable, options, assets, libraries);
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

absl::StatusOr<std::string> SignPackage(std::string_view unsigned_package,
                                        std::string_view certificate_pem,
                                        std::string_view private_key_pem) {
  if (certificate_pem.size() > 16 * 1024 ||
      private_key_pem.size() > 16 * 1024) {
    return absl::InvalidArgumentError("PEM signing input exceeds 16 KiB");
  }
  ABSL_ASSIGN_OR_RETURN(const auto inspected, InspectPackage(unsigned_package));
  if (inspected.signed_package) {
    return absl::InvalidArgumentError("SIS package is already signed");
  }
  const std::unique_ptr<BIO, decltype(&BIO_free)> certificate_input(
      BIO_new_mem_buf(certificate_pem.data(),
                      static_cast<int>(certificate_pem.size())),
      BIO_free);
  const std::unique_ptr<BIO, decltype(&BIO_free)> key_input(
      BIO_new_mem_buf(private_key_pem.data(),
                      static_cast<int>(private_key_pem.size())),
      BIO_free);
  if (!certificate_input || !key_input) {
    return absl::InternalError("OpenSSL input allocation failed");
  }
  const std::unique_ptr<X509, decltype(&X509_free)> certificate(
      PEM_read_bio_X509(certificate_input.get(), nullptr, nullptr, nullptr),
      X509_free);
  const std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(
      PEM_read_bio_PrivateKey(key_input.get(), nullptr, nullptr, nullptr),
      EVP_PKEY_free);
  if (!certificate || !key || EVP_PKEY_base_id(key.get()) != EVP_PKEY_RSA ||
      X509_check_private_key(certificate.get(), key.get()) != 1) {
    return absl::InvalidArgumentError(
        "Expected matching RSA PEM certificate and key");
  }
  const int certificate_length = i2d_X509(certificate.get(), nullptr);
  if (certificate_length <= 0 || certificate_length > 16 * 1024) {
    return absl::InvalidArgumentError(
        "X.509 certificate is too large or invalid");
  }
  std::string der(static_cast<size_t>(certificate_length), '\0');
  auto* absl_nonnull der_ptr = reinterpret_cast<unsigned char*>(der.data());
  if (i2d_X509(certificate.get(), &der_ptr) != certificate_length) {
    return absl::InternalError("X.509 DER serialization failed");
  }
  Reader root(unsigned_package.substr(16));
  Reader contents(root.Take(12).payload);
  const View controller_crc = contents.Take(34);
  const View data_crc = contents.Take(35);
  const View compressed = contents.Take(3);
  const View data = contents.Take(30);
  if (!root.Finish().ok() || !contents.Finish().ok() ||
      !controller_crc.raw.size() || !data_crc.raw.size()) {
    return absl::DataLossError("Malformed canonical SIS contents");
  }
  ABSL_ASSIGN_OR_RETURN(const auto raw_controller,
                        Uncompressed(compressed.payload));
  // SignSIS hashes the controller payload through the install block, excluding
  // the signature chain and trailing data index.
  Reader payload_reader(raw_controller);
  const View controller_field = payload_reader.Take(13);
  Reader fields(controller_field.payload);
  for (const uint32_t type : {14u, 16u, 15u, 17u, 19u, 28u}) {
    fields.Take(type);
  }
  ABSL_RETURN_IF_ERROR(fields.status());
  const std::string_view prefix =
      controller_field.payload.substr(0, fields.position());
  const View index = fields.Take(40);
  ABSL_RETURN_IF_ERROR(fields.Finish());
  const std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> context(
      EVP_MD_CTX_new(), EVP_MD_CTX_free);
  if (!context ||
      EVP_DigestSignInit(context.get(), nullptr, EVP_sha1(), nullptr,
                         key.get()) != 1 ||
      EVP_DigestSignUpdate(context.get(), prefix.data(), prefix.size()) != 1) {
    return absl::InternalError("RSA SHA-1 signing setup failed");
  }
  size_t signature_length = 0;
  if (EVP_DigestSignFinal(context.get(), nullptr, &signature_length) != 1 ||
      signature_length > 1024) {
    return absl::InternalError("RSA signature sizing failed");
  }
  std::string signature(signature_length, '\0');
  if (EVP_DigestSignFinal(context.get(),
                          reinterpret_cast<unsigned char*>(signature.data()),
                          &signature_length) != 1) {
    return absl::InternalError("RSA signing failed");
  }
  signature.resize(signature_length);
  const std::string chain =
      Field(39, Array(36, Field(36,
                                Field(38, String("1.2.840.113549.1.1.5")) +
                                    Field(37, signature),
                                true)) +
                    Field(22, Field(37, der)));
  const std::string signed_controller =
      Field(13, std::string(prefix) + chain + std::string(index.raw));
  const std::string signed_compressed = Compressed(signed_controller);
  std::string result(unsigned_package.substr(0, 16));
  result += Field(12, Field(34, Half(Crc16(signed_compressed))) +
                          std::string(data_crc.raw) + signed_compressed +
                          std::string(data.raw));
  if (result.size() > kMaxPackage) {
    return absl::ResourceExhaustedError("Signed SIS exceeds size limit");
  }
  return result;
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
  ABSL_RETURN_IF_ERROR(root.Finish());
  const View controller_crc = contents.Take(34);
  const View data_crc = contents.Take(35);
  const View compressed = contents.Take(3);
  const View data = contents.Take(30);
  ABSL_RETURN_IF_ERROR(contents.Finish());
  ABSL_RETURN_IF_ERROR(CheckCrc(controller_crc, compressed));
  ABSL_RETURN_IF_ERROR(CheckCrc(data_crc, data));
  ABSL_ASSIGN_OR_RETURN(const auto controller_bytes,
                        Uncompressed(compressed.payload));
  // A signed package differs from this writer's canonical package only by one
  // RSA/SHA-1 certificate chain inserted immediately before the data index.
  // Verify the chain, remove it, then run the ordinary complete inspection.
  Reader signature_root(controller_bytes);
  const View signed_controller = signature_root.Take(13);
  Reader signed_fields(signed_controller.payload);
  for (const uint32_t type : {14u, 16u, 15u, 17u, 19u, 28u}) {
    signed_fields.Take(type);
  }
  ABSL_RETURN_IF_ERROR(signed_fields.status());
  if (signed_fields.PeekType() == 39) {
    const std::string_view signed_prefix =
        signed_controller.payload.substr(0, signed_fields.position());
    Reader chain(signed_fields.Take(39).payload);
    ABSL_ASSIGN_OR_RETURN(const auto signature_element,
                          Single(chain.Take(2).payload, 36));
    Reader signature(signature_element);
    Reader algorithm(signature.Take(38).payload);
    const auto oid = Ascii(algorithm.Take(1).payload);
    const View signature_blob = signature.Take(37);
    Reader certificate_chain(chain.Take(22).payload);
    const View certificate_blob = certificate_chain.Take(37);
    const View data_index = signed_fields.Take(40);
    if (!oid.ok() || *oid != "1.2.840.113549.1.1.5" ||
        !algorithm.Finish().ok() || !signature.Finish().ok() ||
        !certificate_chain.Finish().ok() || !chain.Finish().ok() ||
        !signed_fields.Finish().ok() || !signature_root.Finish().ok()) {
      return absl::DataLossError("Malformed SIS signature chain");
    }
    const auto* absl_nonnull der =
        reinterpret_cast<const unsigned char*>(certificate_blob.payload.data());
    const auto* absl_nonnull der_end = der + certificate_blob.payload.size();
    const std::unique_ptr<X509, decltype(&X509_free)> certificate(
        d2i_X509(nullptr, &der,
                 static_cast<long>(certificate_blob.payload.size())),
        X509_free);
    if (!certificate || der != der_end) {
      return absl::DataLossError("Invalid SIS certificate DER");
    }
    const std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> public_key(
        X509_get_pubkey(certificate.get()), EVP_PKEY_free);
    const std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> verify(
        EVP_MD_CTX_new(), EVP_MD_CTX_free);
    if (!public_key || !verify ||
        EVP_DigestVerifyInit(verify.get(), nullptr, EVP_sha1(), nullptr,
                             public_key.get()) != 1 ||
        EVP_DigestVerifyUpdate(verify.get(), signed_prefix.data(),
                               signed_prefix.size()) != 1 ||
        EVP_DigestVerifyFinal(verify.get(),
                              reinterpret_cast<const unsigned char*>(
                                  signature_blob.payload.data()),
                              signature_blob.payload.size()) != 1) {
      return absl::DataLossError("SIS RSA signature verification failed");
    }
    const std::string unsigned_controller =
        Field(13, std::string(signed_prefix) + std::string(data_index.raw));
    const std::string unsigned_compressed = Compressed(unsigned_controller);
    std::string unsigned_package(bytes.substr(0, 16));
    unsigned_package +=
        Field(12, Field(34, Half(Crc16(unsigned_compressed))) +
                      std::string(data_crc.raw) + unsigned_compressed +
                      std::string(data.raw));
    auto canonical = InspectPackage(unsigned_package);
    ABSL_RETURN_IF_ERROR(canonical.status());
    canonical->signed_package = true;
    return canonical;
  }
  Reader controller_root(controller_bytes);
  Reader controller(controller_root.Take(13).payload);
  ABSL_RETURN_IF_ERROR(controller_root.Finish());
  Reader info(controller.Take(14).payload);
  Reader uid(info.Take(9).payload);
  PackageInfo result;
  result.options.uid = uid.WordValue();
  ABSL_RETURN_IF_ERROR(uid.Finish());
  const auto vendor = Ascii(info.Take(1).payload);
  ABSL_RETURN_IF_ERROR(vendor.status());
  result.options.vendor = *vendor;
  ABSL_ASSIGN_OR_RETURN(const auto name_element,
                        Single(info.Take(2).payload, 1));
  const auto name = Ascii(name_element);
  ABSL_RETURN_IF_ERROR(name.status());
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
  ABSL_RETURN_IF_ERROR(version.Finish());
  ABSL_RETURN_IF_ERROR(info.status());
  controller.Take(16);
  controller.Take(15);
  controller.Take(17);
  controller.Take(19);
  Reader block(controller.Take(28).payload);
  ABSL_ASSIGN_OR_RETURN(const auto descriptions,
                        Elements(block.Take(2).payload, 24, 41));
  if (descriptions.empty() || descriptions.size() > 41) {
    return absl::UnimplementedError("Unsupported SIS file count");
  }
  constexpr std::string_view prefix = "!:\\sys\\bin\\";
  std::vector<std::string> expected_digests;
  for (const auto description : descriptions) {
    Reader file(description);
    ABSL_ASSIGN_OR_RETURN(const auto target, Ascii(file.Take(1).payload));
    result.files.push_back({target, 0, {}});
    file.Take(1);
    if (file.PeekType() == 41) {
      const View capabilities = file.Take(41);
      if (capabilities.payload.size() != 4) {
        return absl::DataLossError("SIS capability field must be one word");
      }
      result.files.back().capabilities = Read32(capabilities.payload, 0);
    }
    Reader hash(file.Take(25).payload);
    if (hash.WordValue() != 1) {
      return absl::UnimplementedError("Unsupported SIS hash algorithm");
    }
    const View digest = hash.Take(37);
    ABSL_RETURN_IF_ERROR(hash.Finish());
    expected_digests.emplace_back(digest.payload);
    ABSL_RETURN_IF_ERROR(file.status());
  }
  result.target = result.files.front().target;
  if (!result.target.starts_with(prefix)) {
    return absl::UnimplementedError("Unsupported SIS install destination");
  }
  result.options.executable_name = result.target.substr(prefix.size());
  ABSL_RETURN_IF_ERROR(block.status());
  ABSL_RETURN_IF_ERROR(controller.status());
  Reader data_reader(data.payload);
  ABSL_ASSIGN_OR_RETURN(const auto unit,
                        Single(data_reader.Take(2).payload, 31));
  ABSL_RETURN_IF_ERROR(data_reader.Finish());
  Reader unit_reader(unit);
  ABSL_ASSIGN_OR_RETURN(const auto file_data,
                        Elements(unit_reader.Take(2).payload, 32, 41));
  ABSL_RETURN_IF_ERROR(unit_reader.Finish());
  if (file_data.size() != result.files.size()) {
    return absl::DataLossError("SIS file description/data count mismatch");
  }
  std::vector<std::string_view> payloads;
  constexpr char kHex[] = "0123456789abcdef";
  for (size_t index = 0; index < file_data.size(); ++index) {
    Reader payload_reader((file_data)[index]);
    ABSL_ASSIGN_OR_RETURN(const auto payload,
                          Uncompressed(payload_reader.Take(3).payload));
    ABSL_RETURN_IF_ERROR(payload_reader.Finish());
    ABSL_ASSIGN_OR_RETURN(const auto digest, Digest(payload));
    if (digest != expected_digests[index]) {
      return absl::DataLossError("SIS file SHA-1 mismatch");
    }
    payloads.push_back(payload);
    result.files[index].size = static_cast<uint32_t>(payload.size());
    for (const char value : digest) {
      const auto byte = static_cast<unsigned char>(value);
      result.files[index].sha1.push_back(kHex[byte >> 4]);
      result.files[index].sha1.push_back(kHex[byte & 15]);
    }
  }
  std::vector<ApplicationFile> assets;
  std::vector<ApplicationFile> libraries;
  bool libraries_started = false;
  for (size_t index = 1; index < payloads.size(); ++index) {
    if (result.files[index].target.starts_with(prefix)) {
      libraries_started = true;
      libraries.push_back(
          {result.files[index].target, std::string(payloads[index])});
    } else {
      if (libraries_started) {
        return absl::InvalidArgumentError(
            "Application resources must precede bundled DLLs");
      }
      assets.push_back(
          {result.files[index].target, std::string(payloads[index])});
    }
  }
  const auto canonical =
      payloads.size() == 1 ? BuildPackage(payloads[0], result.options)
                           : BuildApplicationPackage(payloads[0], assets,
                                                     result.options, libraries);
  ABSL_RETURN_IF_ERROR(canonical.status());
  if (*canonical != bytes) {
    return absl::UnimplementedError(
        "SIS fields are outside the canonical unsigned application package "
        "profile");
  }
  result.executable_uid = Read32(payloads[0], 8);
  result.executable_size = result.files[0].size;
  result.executable_sha1 = result.files[0].sha1;
  result.application_registered = !assets.empty();
  return result;
}

}  // namespace symbian::sis
