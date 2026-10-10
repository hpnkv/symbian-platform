#include "symbian/device/usb.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include <absl/base/nullability.h>
#include <absl/log/log.h>
#include <absl/status/status.h>
#include <absl/status/status_macros.h>
#include <libusb.h>
#include <openssl/sha.h>

#include "symbian/device/transport.h"

namespace symbian::device {
namespace {
constexpr int kTimeoutMs = 5000;
constexpr uint32_t kMaxContainer = 65536;
constexpr uint32_t kMaxSisBytes = 16 * 1024 * 1024;

std::string UsbError(int code) {
  return libusb_error_name(code);
}

struct Context {
  // Owned libusb context, released on every early return.
  libusb_context* absl_nullable value = nullptr;

  ~Context() {
    if (value != nullptr) {
      libusb_exit(value);
    }
  }
};

struct DeviceList {
  // Referenced device array returned by libusb.
  libusb_device* absl_nullable* absl_nullable value = nullptr;

  ~DeviceList() {
    if (value != nullptr) {
      libusb_free_device_list(value, 1);
    }
  }
};

struct Handle {
  // Open device handle for one serial-matched candidate.
  libusb_device_handle* absl_nullable value = nullptr;

  ~Handle() {
    if (value != nullptr) {
      libusb_close(value);
    }
  }
};

struct Config {
  // Active configuration descriptor owned by this scope.
  libusb_config_descriptor* absl_nullable value = nullptr;

  ~Config() {
    if (value != nullptr) {
      libusb_free_config_descriptor(value);
    }
  }
};

struct Claim {
  // Claimed device handle and interface number.
  libusb_device_handle* absl_nullable handle = nullptr;
  int number = -1;
  // Nonzero alternate setting to restore before release.
  int alt = 0;

  ~Claim() {
    if (number >= 0) {
      if (alt != 0) {
        libusb_set_interface_alt_setting(handle, number, 0);
      }
      libusb_release_interface(handle, number);
    }
  }
};

std::string Anchor(uint16_t vendor, const unsigned char* absl_nonnull serial,
                   int length) {
  char vendor_text[5];
  std::snprintf(vendor_text, sizeof(vendor_text), "%04x", vendor);
  std::string input = std::string(vendor_text) + ":serial:" +
                      std::string(reinterpret_cast<const char*>(serial),
                                  static_cast<size_t>(length));
  unsigned char digest[SHA256_DIGEST_LENGTH];
  SHA256(reinterpret_cast<const unsigned char*>(input.data()), input.size(),
         digest);
  char hex[25];
  for (int i = 0; i < 12; ++i) {
    std::snprintf(hex + 2 * i, 3, "%02x", digest[i]);
  }
  hex[24] = '\0';
  return hex;
}

std::vector<UsbCdcUnion> CdcUnions(const unsigned char* absl_nullable bytes,
                                   int length) {
  std::vector<UsbCdcUnion> result;
  if (bytes == nullptr || length < 0) {
    return result;
  }
  for (int offset = 0; offset + 2 <= length && offset < 4096;) {
    const int size = bytes[offset];
    if (size < 2 || offset + size > length) {
      break;
    }
    if (size >= 5 && bytes[offset + 1] == 0x24 && bytes[offset + 2] == 0x06) {
      UsbCdcUnion association;
      association.master = bytes[offset + 3];
      for (int index = offset + 4; index < offset + size; ++index) {
        association.slaves.push_back(bytes[index]);
      }
      result.push_back(std::move(association));
    }
    offset += size;
  }
  return result;
}

std::vector<UsbInterfaceAssociation> InterfaceAssociations(
    const unsigned char* absl_nullable bytes, int length) {
  std::vector<UsbInterfaceAssociation> result;
  if (bytes == nullptr || length < 0) {
    return result;
  }
  for (int offset = 0; offset + 2 <= length && offset < 4096;) {
    const int size = bytes[offset];
    if (size < 2 || offset + size > length) {
      break;
    }
    if (size >= 8 && bytes[offset + 1] == 0x0b) {
      result.push_back({bytes[offset + 2], bytes[offset + 3], bytes[offset + 4],
                        bytes[offset + 5], bytes[offset + 6]});
    }
    offset += size;
  }
  return result;
}

UsbProbe Map(const libusb_config_descriptor& config) {
  UsbProbe result;
  result.state = "observed";
  result.identity_basis = "usb-serial";
  result.configuration = config.bConfigurationValue;
  result.scope = "descriptor read only; interfaces not claimed or activated";
  for (int i = 0; i < config.bNumInterfaces; ++i) {
    const libusb_interface& group = config.interface[i];
    for (int j = 0; j < group.num_altsetting; ++j) {
      const libusb_interface_descriptor& alt = group.altsetting[j];
      UsbInterfaceDescriptor interface;
      interface.number = alt.bInterfaceNumber;
      interface.alternate_setting = alt.bAlternateSetting;
      interface.class_code = alt.bInterfaceClass;
      interface.subclass_code = alt.bInterfaceSubClass;
      interface.protocol_code = alt.bInterfaceProtocol;
      for (int k = 0; k < alt.bNumEndpoints; ++k) {
        const auto& endpoint = alt.endpoint[k];
        static const char* absl_nonnull types[] = {"control", "isochronous",
                                                   "bulk", "interrupt"};
        interface.endpoints.push_back(
            {endpoint.bEndpointAddress,
             (endpoint.bEndpointAddress & 0x80) ? "in" : "out",
             types[endpoint.bmAttributes & 3], endpoint.wMaxPacketSize});
      }
      result.interfaces.push_back(std::move(interface));
      auto unions = CdcUnions(alt.extra, alt.extra_length);
      result.cdc_unions.insert(result.cdc_unions.end(), unions.begin(),
                               unions.end());
    }
  }
  result.interface_associations =
      InterfaceAssociations(config.extra, config.extra_length);
  return result;
}

struct Endpoints {
  // Interface number and one bulk endpoint in each direction.
  int number = -1;
  uint8_t in = 0;
  uint8_t out = 0;
};

Endpoints FindEndpoints(const libusb_config_descriptor& config, int cls,
                        int subcls, int protocol, int wanted_number = -1) {
  Endpoints result;
  int matches = 0;
  for (int i = 0; i < config.bNumInterfaces; ++i) {
    const auto& group = config.interface[i];
    for (int j = 0; j < group.num_altsetting; ++j) {
      const auto& alt = group.altsetting[j];
      if (alt.bInterfaceClass != cls || alt.bInterfaceSubClass != subcls ||
          alt.bInterfaceProtocol != protocol ||
          (wanted_number >= 0 && alt.bInterfaceNumber != wanted_number)) {
        continue;
      }
      Endpoints candidate{.number = alt.bInterfaceNumber, .in = 0, .out = 0};
      int ins = 0, outs = 0;
      for (int k = 0; k < alt.bNumEndpoints; ++k) {
        const auto& endpoints = alt.endpoint[k];
        if ((endpoints.bmAttributes & 3) != LIBUSB_TRANSFER_TYPE_BULK) {
          continue;
        }
        if (endpoints.bEndpointAddress & 0x80) {
          candidate.in = endpoints.bEndpointAddress;
          ++ins;
        } else {
          candidate.out = endpoints.bEndpointAddress;
          ++outs;
        }
      }
      if (ins == 1 && outs == 1) {
        result = candidate;
        ++matches;
      }
    }
  }
  return matches == 1 ? result : Endpoints{};
}

void Append16(std::vector<unsigned char>* absl_nonnull out, uint16_t value) {
  out->push_back(static_cast<unsigned char>(value));
  out->push_back(static_cast<unsigned char>(value >> 8));
}

void Append32(std::vector<unsigned char>* absl_nonnull out, uint32_t value) {
  Append16(out, static_cast<uint16_t>(value));
  Append16(out, static_cast<uint16_t>(value >> 16));
}

uint16_t Read16(const std::vector<unsigned char>& b, size_t pos) {
  return static_cast<uint16_t>(b[pos] | (b[pos + 1] << 8));
}

uint32_t Read32(const std::vector<unsigned char>& b, size_t pos) {
  return static_cast<uint32_t>(Read16(b, pos) |
                               (uint32_t(Read16(b, pos + 2)) << 16));
}

absl::StatusOr<std::vector<unsigned char>> ReadContainer(
    libusb_device_handle* absl_nonnull handle, uint8_t endpoint,
    uint32_t maximum = kMaxContainer) {
  std::vector<unsigned char> bytes(512);
  int length = 0;
  int result_code = libusb_bulk_transfer(handle, endpoint, bytes.data(), 512,
                                         &length, kTimeoutMs);
  if (result_code != 0) {
    return absl::UnavailableError("USB read: " + UsbError(result_code));
  }
  if (length < 12) {
    return absl::DataLossError("Short PTP container header");
  }
  const uint32_t expected = Read32(bytes, 0);
  if (expected < 12 || expected > maximum ||
      static_cast<uint32_t>(length) > expected) {
    return absl::DataLossError("Invalid PTP container length");
  }
  bytes.resize(expected);
  size_t received = static_cast<size_t>(length);
  while (received < expected) {
    int count = 0;
    result_code = libusb_bulk_transfer(
        handle, endpoint, bytes.data() + received,
        static_cast<int>(std::min<size_t>(16384, expected - received)), &count,
        kTimeoutMs);
    if (result_code != 0) {
      return absl::UnavailableError("USB read: " + UsbError(result_code));
    }
    if (count <= 0) {
      return absl::DataLossError("Empty PTP container chunk");
    }
    received += static_cast<size_t>(count);
  }
  return bytes;
}

struct Reply {
  uint16_t response = 0;
  std::vector<unsigned char> data;
  std::vector<uint32_t> params;
};

absl::Status WriteBulk(libusb_device_handle* absl_nonnull handle,
                       uint8_t endpoint,
                       const std::vector<unsigned char>& bytes) {
  for (size_t offset = 0; offset < bytes.size();) {
    const int length =
        static_cast<int>(std::min<size_t>(16384, bytes.size() - offset));
    int sent = 0;
    if (const int code = libusb_bulk_transfer(
            handle, endpoint, const_cast<unsigned char*>(bytes.data()) + offset,
            length, &sent, kTimeoutMs);
        code != 0 || sent <= 0) {
      return absl::UnavailableError("USB PTP write: " + UsbError(code));
    }
    offset += static_cast<size_t>(sent);
  }
  return absl::OkStatus();
}

std::vector<unsigned char> PtpCommand(uint16_t opcode, uint32_t transaction_id,
                                      const std::vector<uint32_t>& params) {
  std::vector<unsigned char> request;
  Append32(&request, static_cast<uint32_t>(12 + params.size() * 4));
  Append16(&request, 1);
  Append16(&request, opcode);
  Append32(&request, transaction_id);
  for (const uint32_t value : params) {
    Append32(&request, value);
  }
  return request;
}

absl::StatusOr<Reply> PtpResponse(libusb_device_handle* absl_nonnull handle,
                                  const Endpoints& endpoints, uint16_t opcode,
                                  uint32_t transaction_id,
                                  uint32_t max_data = kMaxContainer) {
  auto first = ReadContainer(handle, endpoints.in, max_data);
  ABSL_RETURN_IF_ERROR(first.status());
  // A previous session may have timed out waiting for CloseSession even though
  // the handset later completed it. Only a fresh GetDeviceInfo can discard a
  // bounded number of those late response containers before its own reply.
  for (int stale = 0; opcode == 0x1001 && transaction_id == 0 && stale < 8 &&
                      Read32(*first, 8) != 0 && Read16(*first, 4) == 3;
       ++stale) {
    LOG(WARNING) << "Discarding late MTP response for transaction "
                 << Read32(*first, 8);
    first = ReadContainer(handle, endpoints.in, max_data);
    ABSL_RETURN_IF_ERROR(first.status());
  }
  if (Read32(*first, 8) != transaction_id) {
    return absl::DataLossError("PTP transaction mismatch: expected " +
                               std::to_string(transaction_id) + ", received " +
                               std::to_string(Read32(*first, 8)) + ", type " +
                               std::to_string(Read16(*first, 4)) + ", code " +
                               std::to_string(Read16(*first, 6)));
  }
  std::vector<unsigned char> response = std::move(*first);
  Reply result;
  if (Read16(response, 4) == 2) {
    if (Read16(response, 6) != opcode) {
      return absl::DataLossError("PTP opcode mismatch");
    }
    result.data.assign(response.begin() + 12, response.end());
    ABSL_ASSIGN_OR_RETURN(auto second, ReadContainer(handle, endpoints.in));
    response = std::move(second);
    if (Read32(response, 8) != transaction_id) {
      return absl::DataLossError(
          "PTP response mismatch: expected " + std::to_string(transaction_id) +
          ", received " + std::to_string(Read32(response, 8)));
    }
  }
  if (Read16(response, 4) != 3 || (response.size() - 12) % 4 != 0) {
    return absl::DataLossError("Expected PTP response");
  }
  result.response = Read16(response, 6);
  for (size_t offset = 12; offset < response.size(); offset += 4) {
    result.params.push_back(Read32(response, offset));
  }
  return result;
}

absl::StatusOr<Reply> Ptp(libusb_device_handle* absl_nonnull handle,
                          const Endpoints& endpoints, uint16_t opcode,
                          uint32_t transaction_id,
                          const std::vector<uint32_t>& params = {},
                          uint32_t max_data = kMaxContainer) {
  ABSL_RETURN_IF_ERROR(WriteBulk(handle, endpoints.out,
                                 PtpCommand(opcode, transaction_id, params)));
  return PtpResponse(handle, endpoints, opcode, transaction_id, max_data);
}

absl::StatusOr<Reply> PtpSend(libusb_device_handle* absl_nonnull handle,
                              const Endpoints& endpoints, uint16_t opcode,
                              uint32_t transaction_id,
                              const std::vector<uint32_t>& params,
                              const std::vector<unsigned char>& payload) {
  ABSL_RETURN_IF_ERROR(WriteBulk(handle, endpoints.out,
                                 PtpCommand(opcode, transaction_id, params)));
  std::vector<unsigned char> data;
  data.reserve(12 + payload.size());
  Append32(&data, static_cast<uint32_t>(12 + payload.size()));
  Append16(&data, 2);
  Append16(&data, opcode);
  Append32(&data, transaction_id);
  data.insert(data.end(), payload.begin(), payload.end());
  ABSL_RETURN_IF_ERROR(WriteBulk(handle, endpoints.out, data));
  return PtpResponse(handle, endpoints, opcode, transaction_id);
}

void AppendUtf8(std::string* absl_nonnull text, uint32_t codepoint) {
  if (codepoint < 0x80) {
    text->push_back(static_cast<char>(codepoint));
  } else if (codepoint < 0x800) {
    text->push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
    text->push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
  } else if (codepoint < 0x10000) {
    text->push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
    text->push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
    text->push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
  } else {
    text->push_back(static_cast<char>(0xf0 | (codepoint >> 18)));
    text->push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
    text->push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
    text->push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
  }
}

class Cursor {
 public:
  explicit Cursor(const std::vector<unsigned char>& bytes) : bytes_(bytes) {}

  absl::StatusOr<uint16_t> U16() {
    if (offset_ + 2 > bytes_.size()) {
      return absl::DataLossError("Truncated PTP value");
    }
    const uint16_t result = Read16(bytes_, offset_);
    offset_ += 2;
    return result;
  }

  absl::StatusOr<uint32_t> U32() {
    if (offset_ + 4 > bytes_.size()) {
      return absl::DataLossError("Truncated PTP value");
    }
    const uint32_t result = Read32(bytes_, offset_);
    offset_ += 4;
    return result;
  }

  absl::StatusOr<uint64_t> U64() {
    ABSL_ASSIGN_OR_RETURN(auto low, U32());
    ABSL_ASSIGN_OR_RETURN(auto high, U32());
    return uint64_t(low) | (uint64_t(high) << 32);
  }

  absl::StatusOr<std::string> String() {
    if (offset_ >= bytes_.size()) {
      return absl::DataLossError("Truncated PTP string");
    }
    const size_t count = bytes_[offset_++];
    if (offset_ + 2 * count > bytes_.size()) {
      return absl::DataLossError("Truncated PTP string");
    }
    std::string result;
    for (size_t i = 0; i < count && i < 256; ++i) {
      const uint16_t value = Read16(bytes_, offset_ + 2 * i);
      if (value == 0) {
        break;
      }
      uint32_t codepoint = value;
      if (value >= 0xd800 && value <= 0xdbff && i + 1 < count) {
        if (const uint16_t low = Read16(bytes_, offset_ + 2 * (i + 1));
            low >= 0xdc00 && low <= 0xdfff) {
          codepoint = 0x10000u + ((uint32_t(value) - 0xd800u) << 10) +
                      (uint32_t(low) - 0xdc00u);
          ++i;
        } else {
          codepoint = 0xfffd;
        }
      } else if (value >= 0xd800 && value <= 0xdfff) {
        codepoint = 0xfffd;
      }
      AppendUtf8(&result, codepoint < 32 ? '?' : codepoint);
    }
    offset_ += 2 * count;
    return result;
  }

  absl::Status Skip(size_t count) {
    if (offset_ + count > bytes_.size()) {
      return absl::DataLossError("Truncated PTP value");
    }
    offset_ += count;
    return absl::OkStatus();
  }

  absl::StatusOr<std::vector<uint32_t>> Array(int width) {
    ABSL_ASSIGN_OR_RETURN(auto count, U32());
    if (count > 4096) {
      return absl::ResourceExhaustedError("Oversize PTP array");
    }
    std::vector<uint32_t> values;
    for (uint32_t i = 0; i < count; ++i) {
      if (width == 2) {
        ABSL_ASSIGN_OR_RETURN(auto v, U16());
        values.push_back(v);
      } else {
        ABSL_ASSIGN_OR_RETURN(auto v, U32());
        values.push_back(v);
      }
    }
    return values;
  }

 private:
  const std::vector<unsigned char>& bytes_;
  size_t offset_ = 0;
};

absl::StatusOr<MtpDeviceInfo> DeviceInfo(
    const std::vector<unsigned char>& data) {
  Cursor c(data);
  ABSL_ASSIGN_OR_RETURN(auto version, c.U16());
  ABSL_ASSIGN_OR_RETURN(auto ext, c.U32());
  ABSL_ASSIGN_OR_RETURN(auto ext_version, c.U16());
  ABSL_ASSIGN_OR_RETURN(auto ext_name, c.String());
  ABSL_ASSIGN_OR_RETURN(auto mode, c.U16());
  ABSL_ASSIGN_OR_RETURN(auto operations, c.Array(2));
  for (int i = 0; i < 4; ++i) {
    ABSL_RETURN_IF_ERROR(c.Array(2).status());
  }
  ABSL_ASSIGN_OR_RETURN(auto manufacturer, c.String());
  ABSL_ASSIGN_OR_RETURN(auto model, c.String());
  ABSL_ASSIGN_OR_RETURN(auto device_version, c.String());
  ABSL_RETURN_IF_ERROR(c.String().status());
  operations.resize(std::min<size_t>(operations.size(), 128));
  return MtpDeviceInfo{version,      ext,   ext_version,
                       ext_name,     mode,  operations,
                       manufacturer, model, device_version};
}

absl::StatusOr<MtpStorageInfo> StorageInfo(
    const std::vector<unsigned char>& data, uint32_t id) {
  Cursor c(data);
  ABSL_ASSIGN_OR_RETURN(auto type, c.U16());
  ABSL_ASSIGN_OR_RETURN(auto fs, c.U16());
  ABSL_ASSIGN_OR_RETURN(auto access, c.U16());
  ABSL_ASSIGN_OR_RETURN(auto total, c.U64());
  ABSL_ASSIGN_OR_RETURN(auto free, c.U64());
  ABSL_ASSIGN_OR_RETURN(auto images, c.U32());
  auto description = c.String();
  ABSL_RETURN_IF_ERROR(description.status());
  ABSL_ASSIGN_OR_RETURN(auto label, c.String());
  MtpStorageInfo result;
  result.id = id;
  result.storage_type = type;
  result.filesystem_type = fs;
  result.access_capability = access;
  result.total_bytes = total;
  result.free_bytes = free;
  result.free_images = images;
  result.description = *description;
  result.volume_label = label;
  return result;
}

UsbProbe Mtp(libusb_device_handle* absl_nonnull handle,
             const libusb_config_descriptor& config, uint32_t limit) {
  UsbProbe result;
  Endpoints endpoints = FindEndpoints(config, 6, 1, 1);
  if (endpoints.number < 0) {
    result.state = "imaging-interface-unavailable";
    return result;
  }
  Claim claim{.handle = handle};
  if (const int result_code = libusb_claim_interface(handle, endpoints.number);
      result_code != 0) {
    result.state = "claim-failed";
    result.detail = UsbError(result_code);
    return result;
  }
  claim.number = endpoints.number;
  auto info_reply = Ptp(handle, endpoints, 0x1001, 0);
  if (!info_reply.ok()) {
    result.state = "probe-error";
    result.detail = info_reply.status().ToString();
    return result;
  }
  if (info_reply->response != 0x2001) {
    result.state = "device-info-rejected";
    result.response_code = info_reply->response;
    return result;
  }
  auto info = DeviceInfo(info_reply->data);
  if (!info.ok()) {
    result.state = "decode-error";
    result.detail = info.status().ToString();
    return result;
  }
  result.device_info = *info;
  auto opened = Ptp(handle, endpoints, 0x1002, 0, {1});
  if (!opened.ok()) {
    result.state = "probe-error";
    result.detail = opened.status().ToString();
    return result;
  }
  if (opened->response != 0x2001) {
    result.state = "session-rejected";
    result.response_code = opened->response;
    return result;
  }
  uint32_t transaction_id = 1;
  result.state = "connected";
  result.transport = "mtp-usb";
  result.interface_number = endpoints.number;
  result.scope = "read-only metadata";
  if (auto ids_reply = Ptp(handle, endpoints, 0x1004, transaction_id++);
      !ids_reply.ok()) {
    result.state = "storage-list-error";
    result.detail = ids_reply.status().ToString();
  } else if (ids_reply->response != 0x2001) {
    result.state = "storage-list-rejected";
    result.response_code = ids_reply->response;
  } else {
    Cursor c(ids_reply->data);
    if (auto ids = c.Array(4); !ids.ok()) {
      result.state = "decode-error";
      result.detail = ids.status().ToString();
    } else {
      result.storage_count = ids->size();
      for (size_t i = 0; i < std::min<size_t>(ids->size(), 16); ++i) {
        const uint32_t id = (*ids)[i];
        auto reply = Ptp(handle, endpoints, 0x1005, transaction_id++, {id});
        if (!reply.ok()) {
          MtpStorageInfo record;
          record.id = id;
          record.error = reply.status().ToString();
          result.storage.push_back(std::move(record));
          continue;
        }
        if (reply->response != 0x2001) {
          MtpStorageInfo record;
          record.id = id;
          record.response_code = reply->response;
          result.storage.push_back(std::move(record));
          continue;
        }
        auto storage = StorageInfo(reply->data, id);
        MtpStorageInfo record;
        if (storage.ok()) {
          record = *storage;
        } else {
          record.id = id;
          record.error = storage.status().ToString();
        }
        if (limit > 0) {
          if (auto handles = Ptp(handle, endpoints, 0x1007, transaction_id++,
                                 {id, 0, 0xffffffff});
              !handles.ok()) {
            record.root_listing_error = handles.status().ToString();
          } else if (handles->response != 0x2001) {
            record.root_listing_response_code = handles->response;
          } else {
            Cursor hc(handles->data);
            if (auto values = hc.Array(4); !values.ok()) {
              record.root_listing_error = values.status().ToString();
            } else {
              record.root_object_count = values->size();
              for (size_t j = 0; j < std::min<size_t>(values->size(), limit);
                   ++j) {
                const uint32_t handle_id = (*values)[j];
                auto object = Ptp(handle, endpoints, 0x1008, transaction_id++,
                                  {handle_id});
                MtpObjectInfo entry;
                entry.handle = handle_id;
                if (!object.ok()) {
                  entry.error = object.status().ToString();
                } else if (object->response != 0x2001) {
                  entry.response_code = object->response;
                } else if (object->data.size() < 52) {
                  entry.error = "truncated object info";
                } else {
                  entry.format_code = Read16(object->data, 4);
                  entry.size_bytes = Read32(object->data, 8);
                  Cursor oc(object->data);
                  if (const auto skipped = oc.Skip(52); !skipped.ok()) {
                    entry.error = skipped.ToString();
                  } else {
                    if (auto name = oc.String(); name.ok()) {
                      entry.name = *name;
                    } else {
                      entry.error = name.status().ToString();
                    }
                  }
                }
                record.root_objects.push_back(std::move(entry));
              }
            }
          }
        }
        result.storage.push_back(std::move(record));
      }
    }
  }
  auto closed = Ptp(handle, endpoints, 0x1003, transaction_id++);
  result.session_closed = closed.ok() && closed->response == 0x2001;
  return result;
}

absl::Status RequireSuccess(const Reply& reply,
                            const char* absl_nonnull operation) {
  if (reply.response == 0x2001) {
    return absl::OkStatus();
  }
  char code[16];
  std::snprintf(code, sizeof(code), "0x%04x", reply.response);
  return absl::FailedPreconditionError(
      std::string(operation) + " rejected by MTP device (" + code + ")");
}

const char* absl_nonnull PtpOperationName(uint16_t opcode) {
  switch (opcode) {
    case 0x1001:
      return "GetDeviceInfo";
    case 0x1002:
      return "OpenSession";
    case 0x1004:
      return "GetStorageIDs";
    case 0x1005:
      return "GetStorageInfo";
    case 0x1007:
      return "GetObjectHandles";
    case 0x1008:
      return "GetObjectInfo";
    case 0x1009:
      return "GetObject";
    default:
      return "request";
  }
}

absl::StatusOr<Reply> CheckedPtp(libusb_device_handle* absl_nonnull handle,
                                 const Endpoints& endpoints, uint16_t opcode,
                                 uint32_t transaction_id,
                                 const std::vector<uint32_t>& params = {},
                                 uint32_t max_data = kMaxContainer) {
  auto reply = Ptp(handle, endpoints, opcode, transaction_id, params, max_data);
  ABSL_RETURN_IF_ERROR(reply.status());
  char operation[64];
  std::snprintf(operation, sizeof(operation), "MTP %s (0x%04x)",
                PtpOperationName(opcode), opcode);
  if (auto status = RequireSuccess(*reply, operation); !status.ok()) {
    LOG(WARNING) << status;
    return status;
  }
  return reply;
}

absl::StatusOr<MtpObjectInfo> ReadObjectInfo(
    libusb_device_handle* absl_nonnull handle, const Endpoints& endpoints,
    uint32_t transaction_id, uint32_t object_handle) {
  ABSL_ASSIGN_OR_RETURN(auto reply, Ptp(handle, endpoints, 0x1008,
                                        transaction_id, {object_handle}));
  char context[64];
  std::snprintf(context, sizeof(context), "MTP GetObjectInfo handle 0x%08x",
                object_handle);
  if (reply.response == 0x2002) {
    return absl::NotFoundError(std::string(context) +
                               " returned response 0x2002");
  }
  ABSL_RETURN_IF_ERROR(RequireSuccess(reply, context));
  if (reply.data.size() < 52) {
    return absl::DataLossError("Truncated MTP object info");
  }
  MtpObjectInfo object;
  object.handle = object_handle;
  object.format_code = Read16(reply.data, 4);
  object.size_bytes = Read32(reply.data, 8);
  Cursor cursor(reply.data);
  ABSL_RETURN_IF_ERROR(cursor.Skip(52));
  auto name = cursor.String();
  ABSL_RETURN_IF_ERROR(name.status());
  object.name = *name;
  return object;
}

absl::StatusOr<std::vector<uint32_t>> ObjectHandles(
    libusb_device_handle* absl_nonnull handle, const Endpoints& endpoints,
    uint32_t transaction_id, uint32_t storage, uint32_t parent) {
  ABSL_ASSIGN_OR_RETURN(auto reply,
                        CheckedPtp(handle, endpoints, 0x1007, transaction_id,
                                   {storage, 0, parent}));
  Cursor cursor(reply.data);
  return cursor.Array(4);
}

void AppendPtpString(std::vector<unsigned char>* absl_nonnull data,
                     const std::string& value) {
  data->push_back(static_cast<unsigned char>(value.size() + 1));
  for (const char character : value) {
    Append16(data, static_cast<unsigned char>(character));
  }
  Append16(data, 0);
}

std::vector<unsigned char> SisObjectInfo(uint32_t storage, uint32_t parent,
                                         uint32_t size,
                                         const std::string& filename) {
  std::vector<unsigned char> data;
  Append32(&data, storage);
  Append16(&data, 0x3000);  // Undefined object format; SIS has no MTP format.
  Append16(&data, 0);       // No protection flag.
  Append32(&data, size);
  Append16(&data, 0);  // No thumbnail format.
  for (int index = 0; index < 6; ++index) {
    Append32(&data, 0);
  }
  Append32(&data, parent);
  Append16(&data, 0);  // Not an association.
  Append32(&data, 0);
  Append32(&data, 0);
  AppendPtpString(&data, filename);
  data.insert(data.end(), 3,
              0);  // Empty capture date, modified date, keywords.
  return data;
}

std::string Sha256Hex(const std::vector<unsigned char>& bytes) {
  unsigned char digest[SHA256_DIGEST_LENGTH];
  SHA256(bytes.data(), bytes.size(), digest);
  char text[SHA256_DIGEST_LENGTH * 2 + 1];
  for (size_t i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
    std::snprintf(text + i * 2, 3, "%02x", digest[i]);
  }
  text[SHA256_DIGEST_LENGTH * 2] = '\0';
  return text;
}

absl::StatusOr<MtpStageResult> StageOnHandle(
    libusb_device_handle* absl_nonnull handle,
    const libusb_config_descriptor& config,
    const std::vector<unsigned char>& package, const std::string& filename,
    const std::string& expected_sha256,
    std::optional<uint32_t> requested_storage = std::nullopt,
    std::string_view folder = "Installs", bool replace_existing = false) {
  const Endpoints endpoints = FindEndpoints(config, 6, 1, 1);
  if (endpoints.number < 0) {
    return absl::FailedPreconditionError("No unique MTP USB interface");
  }
  Claim claim{.handle = handle};
  const int code = libusb_claim_interface(handle, endpoints.number);
  if (code != 0) {
    return absl::UnavailableError("MTP interface claim: " + UsbError(code));
  }
  claim.number = endpoints.number;
  ABSL_ASSIGN_OR_RETURN(auto info_reply,
                        CheckedPtp(handle, endpoints, 0x1001, 0));
  ABSL_ASSIGN_OR_RETURN(auto device_info, DeviceInfo(info_reply.data));
  for (const uint32_t opcode : std::array<uint32_t, 8>{
           0x1003, 0x1004, 0x1005, 0x1007, 0x1008, 0x1009, 0x100c, 0x100d}) {
    if (std::find(device_info.supported_operation_codes.begin(),
                  device_info.supported_operation_codes.end(),
                  opcode) == device_info.supported_operation_codes.end()) {
      return absl::FailedPreconditionError(
          "Device lacks required MTP transfer operation");
    }
  }
  if (const auto opened = CheckedPtp(handle, endpoints, 0x1002, 0, {1});
      !opened.ok()) {
    return opened.status();
  }
  uint32_t transaction_id = 1;

  // This scope closes the session on every path, including failed transfers.
  struct Session {
    libusb_device_handle* absl_nonnull handle;
    Endpoints endpoints;
    uint32_t* absl_nonnull next_id;

    ~Session() {
      Ptp(handle, endpoints, 0x1003, (*next_id)++).status().IgnoreError();
    }
  } const session{
      .handle = handle, .endpoints = endpoints, .next_id = &transaction_id};

  auto stores = CheckedPtp(handle, endpoints, 0x1004, transaction_id++);
  ABSL_RETURN_IF_ERROR(stores.status());
  Cursor store_cursor(stores->data);
  ABSL_ASSIGN_OR_RETURN(auto ids, store_cursor.Array(4));
  if (ids.size() > 16) {
    return absl::FailedPreconditionError(
        "Too many MTP stores to select safely");
  }
  uint32_t selected_storage = 0;
  uint32_t installs_handle = 0;
  for (const uint32_t id : ids) {
    if (requested_storage.has_value() && id != *requested_storage) {
      continue;
    }
    ABSL_ASSIGN_OR_RETURN(auto reply, CheckedPtp(handle, endpoints, 0x1005,
                                                 transaction_id++, {id}));
    ABSL_ASSIGN_OR_RETURN(auto storage, StorageInfo(reply.data, id));
    if (storage.access_capability != 0 || !storage.free_bytes ||
        storage.free_bytes < package.size() + 16 * 1024 * 1024) {
      continue;
    }
    if (folder.empty()) {
      selected_storage = id;
      installs_handle = 0xffffffff;
      continue;
    }
    auto roots =
        ObjectHandles(handle, endpoints, transaction_id++, id, 0xffffffff);
    ABSL_RETURN_IF_ERROR(roots.status());
    for (const uint32_t object_id : *roots) {
      auto object =
          ReadObjectInfo(handle, endpoints, transaction_id++, object_id);
      ABSL_RETURN_IF_ERROR(object.status());
      if (object->format_code == 0x3001 && object->name == folder) {
        if (selected_storage != 0) {
          return absl::FailedPreconditionError(
              "Several writable MTP Installs folders");
        }
        selected_storage = id;
        installs_handle = object_id;
      }
    }
  }
  if (selected_storage == 0) {
    return absl::FailedPreconditionError("No writable MTP target folder");
  }
  ABSL_ASSIGN_OR_RETURN(auto children,
                        ObjectHandles(handle, endpoints, transaction_id++,
                                      selected_storage, installs_handle));
  uint32_t unreadable_children = 0;
  for (const uint32_t object_id : children) {
    auto object =
        ReadObjectInfo(handle, endpoints, transaction_id++, object_id);
    if (!object.ok()) {
      if (object.status().code() == absl::StatusCode::kNotFound) {
        LOG(WARNING) << "Skipping unreadable MTP Installs child: "
                     << object.status();
        ++unreadable_children;
        continue;
      }
      return object.status();
    }
    if (object->name != filename) {
      continue;
    }
    if (!replace_existing && object->size_bytes != package.size()) {
      return absl::AlreadyExistsError(
          "Different file occupies MTP package path");
    }
    if (object->size_bytes == package.size()) {
      ABSL_ASSIGN_OR_RETURN(
          auto existing, CheckedPtp(handle, endpoints, 0x1009, transaction_id++,
                                    {object_id}, kMaxSisBytes + 12));
      if (Sha256Hex(existing.data) == expected_sha256) {
        return MtpStageResult{selected_storage, object_id, filename, false,
                              unreadable_children};
      }
    }
    if (!replace_existing) {
      return absl::AlreadyExistsError(
          "Different file occupies MTP package path");
    }
    if (std::find(device_info.supported_operation_codes.begin(),
                  device_info.supported_operation_codes.end(),
                  0x100b) == device_info.supported_operation_codes.end()) {
      return absl::FailedPreconditionError("MTP DeleteObject unavailable");
    }
    if (const auto deleted = CheckedPtp(handle, endpoints, 0x100b,
                                        transaction_id++, {object_id});
        !deleted.ok()) {
      return deleted.status();
    }
  }
  const auto object_info =
      SisObjectInfo(selected_storage, installs_handle,
                    static_cast<uint32_t>(package.size()), filename);
  ABSL_ASSIGN_OR_RETURN(
      auto declared, PtpSend(handle, endpoints, 0x100c, transaction_id++,
                             {selected_storage, installs_handle}, object_info));
  ABSL_RETURN_IF_ERROR(RequireSuccess(declared, "SendObjectInfo"));
  if (declared.params.size() < 3 || declared.params[0] != selected_storage ||
      declared.params[2] == 0) {
    return absl::DataLossError(
        "SendObjectInfo omitted the created object handle");
  }
  const uint32_t created = declared.params[2];
  ABSL_ASSIGN_OR_RETURN(auto sent, PtpSend(handle, endpoints, 0x100d,
                                           transaction_id++, {}, package));
  ABSL_RETURN_IF_ERROR(RequireSuccess(sent, "SendObject"));
  auto verified_info =
      ReadObjectInfo(handle, endpoints, transaction_id++, created);
  ABSL_RETURN_IF_ERROR(verified_info.status());
  if (verified_info->name != filename ||
      verified_info->size_bytes != package.size()) {
    return absl::DataLossError("MTP object metadata differs after upload");
  }
  auto readback = CheckedPtp(handle, endpoints, 0x1009, transaction_id++,
                             {created}, kMaxSisBytes + 12);
  ABSL_RETURN_IF_ERROR(readback.status());
  if (Sha256Hex(readback->data) != expected_sha256) {
    return absl::DataLossError("MTP package readback digest differs");
  }
  LOG(INFO) << "MTP SIS upload verified by readback: " << filename
            << ", object handle " << created;
  return MtpStageResult{.storage_id = selected_storage,
                        .object_handle = created,
                        .name = filename,
                        .copied = true,
                        .unreadable_children = unreadable_children};
}

absl::StatusOr<std::vector<unsigned char>> AccessPathOnHandle(
    libusb_device_handle* absl_nonnull handle,
    const libusb_config_descriptor& config, uint32_t storage_id,
    std::string_view relative_path, uint32_t max_bytes,
    std::optional<std::string_view> delete_sha256) {
  const Endpoints endpoints = FindEndpoints(config, 6, 1, 1);
  if (endpoints.number < 0) {
    return absl::FailedPreconditionError("No unique MTP USB interface");
  }
  Claim claim{handle};
  const int code = libusb_claim_interface(handle, endpoints.number);
  if (code != 0) {
    return absl::UnavailableError("MTP interface claim: " + UsbError(code));
  }
  claim.number = endpoints.number;
  ABSL_ASSIGN_OR_RETURN(auto info, CheckedPtp(handle, endpoints, 0x1001, 0));
  ABSL_ASSIGN_OR_RETURN(auto device_info, DeviceInfo(info.data));
  for (const uint32_t opcode :
       std::array<uint32_t, 5>{0x1003, 0x1005, 0x1007, 0x1008, 0x1009}) {
    if (std::find(device_info.supported_operation_codes.begin(),
                  device_info.supported_operation_codes.end(),
                  opcode) == device_info.supported_operation_codes.end()) {
      return absl::FailedPreconditionError(
          "Device lacks required MTP file-read operation");
    }
  }
  if (const auto opened = CheckedPtp(handle, endpoints, 0x1002, 0, {1});
      !opened.ok()) {
    return opened.status();
  }
  uint32_t transaction_id = 1;

  struct Session {
    libusb_device_handle* absl_nonnull handle;
    Endpoints endpoints;
    uint32_t* absl_nonnull next_id;

    ~Session() {
      Ptp(handle, endpoints, 0x1003, (*next_id)++).status().IgnoreError();
    }
  } const session{handle, endpoints, &transaction_id};

  if (const auto storage =
          CheckedPtp(handle, endpoints, 0x1005, transaction_id++, {storage_id});
      !storage.ok()) {
    return storage.status();
  }
  uint32_t parent = 0xffffffff;
  while (!relative_path.empty()) {
    const size_t slash = relative_path.find('/');
    const std::string_view component = relative_path.substr(0, slash);
    auto children =
        ObjectHandles(handle, endpoints, transaction_id++, storage_id, parent);
    ABSL_RETURN_IF_ERROR(children.status());
    if (children->size() > 4096) {
      return absl::ResourceExhaustedError("MTP directory exceeds 4096 objects");
    }
    std::optional<MtpObjectInfo> selected;
    for (const uint32_t object_id : *children) {
      auto object =
          ReadObjectInfo(handle, endpoints, transaction_id++, object_id);
      if (!object.ok()) {
        if (object.status().code() == absl::StatusCode::kNotFound) {
          continue;
        }
        return object.status();
      }
      if (object->name == component) {
        if (selected.has_value()) {
          return absl::FailedPreconditionError("Ambiguous MTP path component");
        }
        selected = std::move(*object);
      }
    }
    if (!selected.has_value()) {
      return absl::NotFoundError("MTP file path unavailable");
    }
    if (slash != std::string_view::npos) {
      if (selected->format_code != 0x3001) {
        return absl::FailedPreconditionError(
            "MTP path component is not a folder");
      }
      parent = selected->handle;
      relative_path.remove_prefix(slash + 1);
      continue;
    }
    if (selected->format_code == 0x3001) {
      return absl::FailedPreconditionError("MTP path names a folder");
    }
    if (selected->size_bytes > max_bytes) {
      return absl::ResourceExhaustedError("MTP file exceeds read limit");
    }
    ABSL_ASSIGN_OR_RETURN(
        auto contents, CheckedPtp(handle, endpoints, 0x1009, transaction_id++,
                                  {selected->handle}, max_bytes + 12));
    if (contents.data.size() != selected->size_bytes) {
      return absl::DataLossError("MTP file size changed during read");
    }
    if (delete_sha256.has_value()) {
      if (Sha256Hex(contents.data) != *delete_sha256) {
        return absl::FailedPreconditionError(
            "MTP deletion digest does not match the file");
      }
      if (std::find(device_info.supported_operation_codes.begin(),
                    device_info.supported_operation_codes.end(),
                    0x100b) == device_info.supported_operation_codes.end()) {
        return absl::UnimplementedError("MTP DeleteObject unavailable");
      }
      ABSL_RETURN_IF_ERROR(CheckedPtp(handle, endpoints, 0x100b,
                                      transaction_id++, {selected->handle})
                               .status());
      // Confirm removal in the same directory; a success response alone does
      // not establish that the object disappeared.
      ABSL_ASSIGN_OR_RETURN(const auto remaining,
                            ObjectHandles(handle, endpoints, transaction_id++,
                                          storage_id, parent));
      if (std::find(remaining.begin(), remaining.end(), selected->handle) !=
          remaining.end()) {
        return absl::DataLossError("MTP object remains after deletion");
      }
      return std::vector<unsigned char>{};
    }
    return std::move(contents.data);
  }
  return absl::InvalidArgumentError("Empty MTP path");
}

UsbProbe Obex(libusb_device_handle* absl_nonnull handle,
              const libusb_config_descriptor& config) {
  UsbProbe result;
  // Interface 9 is selected by the observed PC Suite Services CDC union 8->9.
  bool pair = false;
  for (int i = 0; i < config.bNumInterfaces; ++i) {
    for (int j = 0; j < config.interface[i].num_altsetting; ++j) {
      for (const auto& item :
           CdcUnions(config.interface[i].altsetting[j].extra,
                     config.interface[i].altsetting[j].extra_length)) {
        if (item.master == 8) {
          for (const int slave : item.slaves) {
            if (slave == 9) {
              pair = true;
            }
          }
        }
      }
    }
  }
  if (!pair) {
    result.state = "pc-suite-union-unavailable";
    return result;
  }
  Endpoints endpoints = FindEndpoints(config, 10, 0, 0, 9);
  if (endpoints.number < 0) {
    result.state = "pc-suite-data-unavailable";
    return result;
  }
  Claim claim{.handle = handle};
  int result_code = libusb_claim_interface(handle, endpoints.number);
  if (result_code != 0) {
    result.state = "claim-failed";
    result.detail = UsbError(result_code);
    return result;
  }
  claim.number = endpoints.number;
  result_code = libusb_set_interface_alt_setting(handle, endpoints.number, 1);
  if (result_code != 0) {
    result.state = "alternate-setting-failed";
    result.detail = UsbError(result_code);
    return result;
  }
  claim.alt = 1;
  constexpr std::array<unsigned char, 16> target = {
      0xf9, 0xec, 0x7b, 0xc4, 0x95, 0x3c, 0x11, 0xd2,
      0x98, 0x4e, 0x52, 0x54, 0x00, 0xdc, 0x9e, 0x09};
  std::vector<unsigned char> connect = {0x80, 0,    26,   0x10, 0,
                                        0x04, 0x00, 0x46, 0,    19};
  connect.insert(connect.end(), target.begin(), target.end());
  int sent = 0;
  result_code =
      libusb_bulk_transfer(handle, endpoints.out, connect.data(),
                           static_cast<int>(connect.size()), &sent, kTimeoutMs);
  if (result_code != 0 || sent != static_cast<int>(connect.size())) {
    result.state = "connect-write-error";
    result.detail = UsbError(result_code);
    return result;
  }
  std::array<unsigned char, 1024> response{};
  int count = 0;
  result_code = libusb_bulk_transfer(handle, endpoints.in, response.data(),
                                     response.size(), &count, kTimeoutMs);
  if (result_code != 0) {
    result.state = "connect-read-error";
    result.detail = UsbError(result_code);
    return result;
  }
  if (count < 3 || ((response[1] << 8) | response[2]) > count) {
    result.state = "invalid-connect-response";
    result.response_bytes = count;
    return result;
  }
  result.state = response[0] == 0xa0 ? "connected" : "rejected";
  result.response_code = response[0];
  result.interface_number = endpoints.number;
  result.target = "PC Suite FTP UUID";
  result.scope = "OBEX connect and disconnect only";
  if (response[0] == 0xa0) {
    std::vector<unsigned char> disconnect = {0x81, 0, 3};
    // A server may assign a Connection ID in the Connect response.
    for (size_t pos = 7; pos < static_cast<size_t>(count);) {
      const unsigned char header = response[pos];
      if (header == 0xcb && pos + 5 <= static_cast<size_t>(count)) {
        disconnect = {0x81,
                      0,
                      8,
                      0xcb,
                      response[pos + 1],
                      response[pos + 2],
                      response[pos + 3],
                      response[pos + 4]};
        result.connection_id_present = true;
        break;
      }
      if ((header & 0xc0) == 0xc0) {
        pos += 5;
      } else if ((header & 0xc0) == 0x80) {
        pos += 2;
      } else {
        if (pos + 3 > static_cast<size_t>(count)) {
          break;
        }
        const int header_length = (response[pos + 1] << 8) | response[pos + 2];
        if (header_length < 3) {
          break;
        }
        pos += static_cast<size_t>(header_length);
      }
    }
    result_code = libusb_bulk_transfer(handle, endpoints.out, disconnect.data(),
                                       static_cast<int>(disconnect.size()),
                                       &sent, kTimeoutMs);
    if (result_code == 0 && sent == static_cast<int>(disconnect.size())) {
      result_code = libusb_bulk_transfer(handle, endpoints.in, response.data(),
                                         response.size(), &count, kTimeoutMs);
      result.disconnected =
          result_code == 0 && count >= 3 && response[0] == 0xa0;
      if (result_code != 0) {
        result.disconnect_error = UsbError(result_code);
      } else if (count >= 3) {
        result.disconnect_response_code = response[0];
      }
    } else {
      result.disconnected = false;
      result.disconnect_error = UsbError(result_code);
    }
  }
  const int restore =
      libusb_set_interface_alt_setting(handle, endpoints.number, 0);
  result.alternate_restored = restore == 0;
  if (restore == 0) {
    claim.alt = 0;
  }
  const int released = libusb_release_interface(handle, endpoints.number);
  result.interface_released = released == 0;
  if (released == 0) {
    claim.number = -1;
  }
  return result;
}
}  // namespace

absl::StatusOr<UsbProbe> InspectUsb(uint16_t vendor, uint16_t product,
                                    const std::string& anchor,
                                    const std::string& operation,
                                    uint32_t limit) {
  if (anchor.size() != 24) {
    return absl::InvalidArgumentError("USB serial anchor required");
  }
  if (limit > 128) {
    return absl::InvalidArgumentError("MTP listing limit must be <=128");
  }
  if (operation != "map" && operation != "mtp" && operation != "obex") {
    return absl::InvalidArgumentError("Unsupported USB operation");
  }
  Context context;
  int result_code = libusb_init(&context.value);
  if (result_code != 0) {
    return absl::UnavailableError("libusb init: " + UsbError(result_code));
  }
  DeviceList list;
  const ssize_t count = libusb_get_device_list(context.value, &list.value);
  if (count < 0) {
    return absl::UnavailableError("libusb list: " +
                                  UsbError(static_cast<int>(count)));
  }
  UsbProbe answer;
  int matches = 0;
  for (ssize_t i = 0; i < count; ++i) {
    libusb_device_descriptor descriptor{};
    if (libusb_get_device_descriptor(list.value[i], &descriptor) != 0 ||
        descriptor.idVendor != vendor || descriptor.idProduct != product ||
        descriptor.iSerialNumber == 0) {
      continue;
    }
    Handle handle;
    if (libusb_open(list.value[i], &handle.value) != 0) {
      continue;
    }
    unsigned char serial[256];
    if (const int size = libusb_get_string_descriptor_ascii(
            handle.value, descriptor.iSerialNumber, serial, sizeof(serial));
        size <= 0 || Anchor(vendor, serial, size) != anchor) {
      continue;
    }
    ++matches;
    Config config;
    result_code =
        libusb_get_active_config_descriptor(list.value[i], &config.value);
    if (result_code != 0) {
      answer.state = "configuration-unavailable";
      answer.detail = UsbError(result_code);
      continue;
    }
    if (operation == "map") {
      answer = Map(*config.value);
    } else if (operation == "mtp") {
      answer = Mtp(handle.value, *config.value, limit);
    } else {
      answer = Obex(handle.value, *config.value);
    }
  }
  if (matches > 1) {
    answer = UsbProbe{};
    answer.state = "ambiguous";
  }
  return answer;
}

absl::StatusOr<MtpStageResult> StageMtpSis(uint16_t vendor, uint16_t product,
                                           const std::string& anchor,
                                           const std::string& package_path,
                                           const std::string& filename,
                                           const std::string& expected_sha256) {
  if (anchor.size() != 24) {
    return absl::InvalidArgumentError("USB serial anchor required");
  }
  if (filename.size() > 100 || filename.size() < 5 ||
      filename.find("..") != std::string::npos ||
      !((filename.front() >= 'a' && filename.front() <= 'z') ||
        (filename.front() >= 'A' && filename.front() <= 'Z') ||
        (filename.front() >= '0' && filename.front() <= '9')) ||
      filename.substr(filename.size() - 4) != ".sis" ||
      !std::all_of(filename.begin(), filename.end(), [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
               (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
      })) {
    return absl::InvalidArgumentError("Unsafe MTP SIS filename");
  }
  if (expected_sha256.size() != 64 ||
      !std::all_of(expected_sha256.begin(), expected_sha256.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
      })) {
    return absl::InvalidArgumentError("Expected lowercase SHA-256 required");
  }
  std::ifstream input(package_path, std::ios::binary | std::ios::ate);
  if (!input) {
    return absl::NotFoundError("SIS package cannot be opened");
  }
  const auto size = input.tellg();
  if (size <= 0 || size > kMaxSisBytes) {
    return absl::ResourceExhaustedError("MTP SIS must be at most 16 MiB");
  }
  std::vector<unsigned char> package(static_cast<size_t>(size));
  input.seekg(0);
  input.read(reinterpret_cast<char*>(package.data()), size);
  if (!input || Sha256Hex(package) != expected_sha256) {
    return absl::AbortedError("SIS changed before MTP transfer");
  }
  Context context;
  int code = libusb_init(&context.value);
  if (code != 0) {
    return absl::UnavailableError("libusb init: " + UsbError(code));
  }
  DeviceList list;
  const ssize_t count = libusb_get_device_list(context.value, &list.value);
  if (count < 0) {
    return absl::UnavailableError("libusb list: " +
                                  UsbError(static_cast<int>(count)));
  }
  libusb_device* absl_nullable selected = nullptr;
  for (ssize_t index = 0; index < count; ++index) {
    libusb_device_descriptor descriptor{};
    if (libusb_get_device_descriptor(list.value[index], &descriptor) != 0 ||
        descriptor.idVendor != vendor || descriptor.idProduct != product ||
        descriptor.iSerialNumber == 0) {
      continue;
    }
    Handle candidate;
    if (libusb_open(list.value[index], &candidate.value) != 0) {
      continue;
    }
    unsigned char serial[256];
    if (const int length = libusb_get_string_descriptor_ascii(
            candidate.value, descriptor.iSerialNumber, serial, sizeof(serial));
        length <= 0 || Anchor(vendor, serial, length) != anchor) {
      continue;
    }
    if (selected != nullptr) {
      return absl::FailedPreconditionError("Ambiguous USB serial anchor");
    }
    selected = list.value[index];
  }
  if (selected == nullptr) {
    return absl::NotFoundError("Serial-matched USB device unavailable");
  }
  Handle handle;
  code = libusb_open(selected, &handle.value);
  if (code != 0) {
    return absl::UnavailableError("USB device open: " + UsbError(code));
  }
  Config config;
  code = libusb_get_active_config_descriptor(selected, &config.value);
  if (code != 0) {
    return absl::UnavailableError("USB configuration: " + UsbError(code));
  }
  return StageOnHandle(handle.value, *config.value, package, filename,
                       expected_sha256);
}

static absl::StatusOr<std::vector<unsigned char>> AccessMtpFile(
    uint16_t vendor, uint16_t product, const std::string& anchor,
    uint32_t storage_id, std::string_view relative_path, uint32_t max_bytes,
    std::optional<std::string_view> delete_sha256) {
  if (anchor.size() != 24 || storage_id == 0 || max_bytes == 0 ||
      max_bytes > kMaxSisBytes || relative_path.empty() ||
      relative_path.size() > 255) {
    return absl::InvalidArgumentError("Invalid bounded MTP file request");
  }
  size_t components = 0;
  std::string_view rest = relative_path;
  while (!rest.empty()) {
    const size_t slash = rest.find('/');
    if (const std::string_view part = rest.substr(0, slash);
        part.empty() || part == "." || part == ".." ||
        part.find_first_of("\\:") != std::string_view::npos ||
        part.find('\0') != std::string_view::npos || ++components > 8) {
      return absl::InvalidArgumentError("Unsafe MTP relative path");
    }
    if (slash == std::string_view::npos) {
      break;
    }
    rest.remove_prefix(slash + 1);
    if (rest.empty()) {
      return absl::InvalidArgumentError("Unsafe MTP relative path");
    }
  }
  Context context;
  int code = libusb_init(&context.value);
  if (code != 0) {
    return absl::UnavailableError("libusb init: " + UsbError(code));
  }
  DeviceList list;
  const ssize_t count = libusb_get_device_list(context.value, &list.value);
  if (count < 0) {
    return absl::UnavailableError("libusb list: " +
                                  UsbError(static_cast<int>(count)));
  }
  libusb_device* absl_nullable selected = nullptr;
  for (ssize_t index = 0; index < count; ++index) {
    libusb_device_descriptor descriptor{};
    if (libusb_get_device_descriptor(list.value[index], &descriptor) != 0 ||
        descriptor.idVendor != vendor || descriptor.idProduct != product ||
        descriptor.iSerialNumber == 0) {
      continue;
    }
    Handle candidate;
    if (libusb_open(list.value[index], &candidate.value) != 0) {
      continue;
    }
    unsigned char serial[256];
    if (const int length = libusb_get_string_descriptor_ascii(
            candidate.value, descriptor.iSerialNumber, serial, sizeof(serial));
        length <= 0 || Anchor(vendor, serial, length) != anchor) {
      continue;
    }
    if (selected != nullptr) {
      return absl::FailedPreconditionError("Ambiguous USB serial anchor");
    }
    selected = list.value[index];
  }
  if (selected == nullptr) {
    return absl::NotFoundError("Serial-matched USB device unavailable");
  }
  Handle handle;
  code = libusb_open(selected, &handle.value);
  if (code != 0) {
    return absl::UnavailableError("USB device open: " + UsbError(code));
  }
  Config config;
  code = libusb_get_active_config_descriptor(selected, &config.value);
  if (code != 0) {
    return absl::UnavailableError("USB configuration: " + UsbError(code));
  }
  return AccessPathOnHandle(handle.value, *config.value, storage_id,
                            relative_path, max_bytes, delete_sha256);
}

absl::StatusOr<std::vector<unsigned char>> ReadMtpFile(
    uint16_t vendor, uint16_t product, const std::string& anchor,
    uint32_t storage_id, std::string_view relative_path, uint32_t max_bytes) {
  return AccessMtpFile(vendor, product, anchor, storage_id, relative_path,
                       max_bytes, std::nullopt);
}

absl::Status DeleteMtpFile(uint16_t vendor, uint16_t product,
                           const std::string& anchor, uint32_t storage_id,
                           std::string_view relative_path,
                           std::string_view expected_sha256) {
  if (expected_sha256.size() != 64 ||
      !std::all_of(expected_sha256.begin(), expected_sha256.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
      })) {
    return absl::InvalidArgumentError("Expected lowercase SHA-256 required");
  }
  return AccessMtpFile(vendor, product, anchor, storage_id, relative_path,
                       kMaxSisBytes, expected_sha256)
      .status();
}

}  // namespace symbian::device

namespace symbian::device {
namespace {
const char* absl_nonnull CompletionStatus(libusb_transfer_status status) {
  switch (status) {
    case LIBUSB_TRANSFER_COMPLETED:
      return "completed";
    case LIBUSB_TRANSFER_ERROR:
      return "error";
    case LIBUSB_TRANSFER_TIMED_OUT:
      return "timed-out";
    case LIBUSB_TRANSFER_CANCELLED:
      return "cancelled";
    case LIBUSB_TRANSFER_STALL:
      return "stalled";
    case LIBUSB_TRANSFER_NO_DEVICE:
      return "device-unavailable";
    case LIBUSB_TRANSFER_OVERFLOW:
      return "overflow";
  }
  return "unknown";
}

absl::Status TransferStatus(int result_code,
                            const char* absl_nonnull operation) {
  if (result_code == 0) {
    return absl::OkStatus();
  }
  return absl::UnavailableError(std::string(operation) + ": " +
                                UsbError(result_code));
}

absl::Status CheckTimeout(int timeout_ms) {
  if (timeout_ms < 1 || timeout_ms > 30000) {
    return absl::InvalidArgumentError(
        "USB transfer timeout must be 1..30000 ms");
  }
  return absl::OkStatus();
}

absl::Status CheckLength(int length) {
  if (length < 1 || length > 65536) {
    return absl::InvalidArgumentError(
        "USB transfer length must be 1..65536 bytes");
  }
  return absl::OkStatus();
}
}  // namespace

struct UsbSession::Impl {
  struct Pending {
    // Owning session and transfer identifier used by the libusb callback.
    Impl* absl_nullable owner = nullptr;
    uint64_t id = 0;
    // Libusb transfer and backing buffer; both live until callback completion.
    libusb_transfer* absl_nullable transfer = nullptr;
    std::vector<unsigned char> bytes;
    // Transfer layout controls payload extraction from the backing buffer.
    bool inbound = false;
    bool control = false;
    // Optional promise used by the asyncio Future path.
    std::shared_ptr<symbian::concurrency::Promise<UsbCompletion>> confirmation;

    ~Pending() {
      if (transfer != nullptr) {
        libusb_free_transfer(transfer);
      }
    }
  };

  // Owned libusb context, opened device, and descriptor snapshot.
  libusb_context* absl_nullable context = nullptr;
  libusb_device_handle* absl_nullable handle = nullptr;
  libusb_config_descriptor* absl_nullable config = nullptr;
  // Pump and state locks keep libusb event processing and callback state safe.
  mutable std::mutex pump_mutex;
  mutable std::mutex state_mutex;
  // Close state and claimed interface alternate settings.
  bool closed = false;
  bool closing = false;
  std::map<int, int> claimed;
  // Outstanding transfers and completions for explicit transfer IDs.
  std::map<uint64_t, std::unique_ptr<Pending>> pending;
  std::vector<UsbCompletion> completed;
  // Futures are settled only after libusb returns from its callback.
  std::vector<
      std::pair<std::shared_ptr<symbian::concurrency::Promise<UsbCompletion>>,
                UsbCompletion>>
      ready_futures;
  // Monotonic transfer identifier within this session.
  uint64_t next_id = 1;

  static void Callback(libusb_transfer* absl_nonnull transfer) {
    auto* absl_nonnull raw = static_cast<Pending*>(transfer->user_data);
    Impl* absl_nonnull owner = raw->owner;
    const std::lock_guard<std::mutex> lock(owner->state_mutex);
    const auto it = owner->pending.find(raw->id);
    if (it == owner->pending.end()) {
      return;
    }
    std::unique_ptr<Pending> value = std::move(it->second);
    owner->pending.erase(it);
    UsbCompletion completion;
    completion.id = value->id;
    completion.status = CompletionStatus(transfer->status);
    completion.actual_length = transfer->actual_length;
    if (const size_t expected_out =
            value->bytes.size() -
            (value->control ? LIBUSB_CONTROL_SETUP_SIZE : 0);
        !value->inbound && completion.status == "completed" &&
        static_cast<size_t>(transfer->actual_length) != expected_out) {
      completion.status = "short-transfer";
    }
    if (value->inbound && transfer->actual_length > 0) {
      const size_t offset = value->control ? LIBUSB_CONTROL_SETUP_SIZE : 0;
      const size_t length =
          std::min<size_t>(static_cast<size_t>(transfer->actual_length),
                           value->bytes.size() - offset);
      completion.data.assign(
          reinterpret_cast<const char*>(value->bytes.data() + offset), length);
    }
    if (value->confirmation != nullptr) {
      owner->ready_futures.emplace_back(std::move(value->confirmation),
                                        std::move(completion));
    } else {
      owner->completed.push_back(std::move(completion));
    }
  }

  bool SettleReadyFutures() {
    std::vector<
        std::pair<std::shared_ptr<symbian::concurrency::Promise<UsbCompletion>>,
                  UsbCompletion>>
        ready;
    {
      const std::lock_guard<std::mutex> lock(state_mutex);
      ready.swap(ready_futures);
    }
    const bool settled = !ready.empty();
    for (auto& [confirmation, completion] : ready) {
      if (completion.status == "completed") {
        confirmation->SetValue(std::move(completion));
      } else if (completion.status == "cancelled") {
        confirmation->SetError(absl::CancelledError("USB transfer cancelled"));
      } else if (completion.status == "timed-out") {
        confirmation->SetError(
            absl::DeadlineExceededError("USB transfer timed out"));
      } else {
        confirmation->SetError(
            absl::UnavailableError("USB transfer: " + completion.status));
      }
    }
    return settled;
  }
};

UsbSession::UsbSession(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

UsbSession::~UsbSession() {
  if (!Close().ok()) {
    impl_.release();
  }
}

absl::StatusOr<std::shared_ptr<UsbSession>> UsbSession::Open(
    uint16_t vendor, uint16_t product, const std::string& serial_anchor) {
  if (serial_anchor.size() != 24) {
    return absl::InvalidArgumentError("USB serial anchor required");
  }
  auto impl = std::make_unique<Impl>();
  int result_code = libusb_init(&impl->context);
  if (result_code != 0) {
    return TransferStatus(result_code, "libusb init");
  }
  DeviceList list;
  const ssize_t count = libusb_get_device_list(impl->context, &list.value);
  if (count < 0) {
    libusb_exit(impl->context);
    return TransferStatus(static_cast<int>(count), "libusb list");
  }
  int matches = 0;
  for (ssize_t i = 0; i < count; ++i) {
    libusb_device_descriptor descriptor{};
    if (libusb_get_device_descriptor(list.value[i], &descriptor) != 0 ||
        descriptor.idVendor != vendor || descriptor.idProduct != product ||
        descriptor.iSerialNumber == 0) {
      continue;
    }
    libusb_device_handle* absl_nullable candidate = nullptr;
    if (libusb_open(list.value[i], &candidate) != 0) {
      continue;
    }
    unsigned char serial[256];
    if (const int size = libusb_get_string_descriptor_ascii(
            candidate, descriptor.iSerialNumber, serial, sizeof(serial));
        size > 0 && Anchor(vendor, serial, size) == serial_anchor) {
      ++matches;
      if (matches == 1) {
        impl->handle = candidate;
        result_code =
            libusb_get_active_config_descriptor(list.value[i], &impl->config);
        if (result_code != 0) {
          libusb_close(candidate);
          libusb_free_device_list(list.value, 1);
          list.value = nullptr;
          libusb_exit(impl->context);
          return TransferStatus(result_code, "active configuration");
        }
        continue;
      }
    }
    libusb_close(candidate);
  }
  if (matches != 1) {
    if (impl->config != nullptr) {
      libusb_free_config_descriptor(impl->config);
    }
    if (impl->handle != nullptr) {
      libusb_close(impl->handle);
    }
    libusb_free_device_list(list.value, 1);
    list.value = nullptr;
    libusb_exit(impl->context);
    return matches == 0
               ? absl::NotFoundError("Serial-matched USB device unavailable")
               : absl::FailedPreconditionError("USB identity ambiguous");
  }
  return std::shared_ptr<UsbSession>(new UsbSession(std::move(impl)));
}

absl::StatusOr<UsbProbe> UsbSession::Descriptors() const {
  const std::lock_guard<std::mutex> lock(impl_->pump_mutex);
  if (impl_->closed || impl_->closing) {
    return absl::FailedPreconditionError("USB session closed");
  }
  return Map(*impl_->config);
}

absl::Status UsbSession::Claim(int number) {
  if (number < 0 || number > 255) {
    return absl::InvalidArgumentError("Invalid interface number");
  }
  const std::lock_guard<std::mutex> lock(impl_->pump_mutex);
  if (impl_->closed || impl_->closing) {
    return absl::FailedPreconditionError("USB session closed");
  }
  if (impl_->claimed.contains(number)) {
    return absl::AlreadyExistsError("Interface already claimed");
  }
  if (const int result_code = libusb_claim_interface(impl_->handle, number);
      result_code != 0) {
    return TransferStatus(result_code, "claim interface");
  }
  impl_->claimed[number] = 0;
  return absl::OkStatus();
}

absl::Status UsbSession::SetAlternate(int number, int alternate) {
  if (alternate < 0 || alternate > 255) {
    return absl::InvalidArgumentError("Invalid alternate setting");
  }
  const std::lock_guard<std::mutex> lock(impl_->pump_mutex);
  if (impl_->closed || impl_->closing) {
    return absl::FailedPreconditionError("USB session closed");
  }
  if (!impl_->claimed.contains(number)) {
    return absl::FailedPreconditionError("Claim interface first");
  }
  if (const int result_code =
          libusb_set_interface_alt_setting(impl_->handle, number, alternate);
      result_code != 0) {
    return TransferStatus(result_code, "set alternate setting");
  }
  impl_->claimed[number] = alternate;
  return absl::OkStatus();
}

absl::Status UsbSession::Release(int number) {
  const std::lock_guard<std::mutex> lock(impl_->pump_mutex);
  if (impl_->closed || impl_->closing) {
    return absl::FailedPreconditionError("USB session closed");
  }
  const auto it = impl_->claimed.find(number);
  if (it == impl_->claimed.end()) {
    return absl::FailedPreconditionError("Interface not claimed");
  }
  {
    const std::lock_guard<std::mutex> state_lock(impl_->state_mutex);
    if (!impl_->pending.empty()) {
      return absl::FailedPreconditionError(
          "Complete or cancel transfers first");
    }
  }
  if (it->second != 0) {
    if (const int result_code =
            libusb_set_interface_alt_setting(impl_->handle, number, 0);
        result_code != 0) {
      return TransferStatus(result_code, "restore alternate setting");
    }
  }
  if (const int result_code = libusb_release_interface(impl_->handle, number);
      result_code != 0) {
    return TransferStatus(result_code, "release interface");
  }
  impl_->claimed.erase(it);
  return absl::OkStatus();
}

absl::StatusOr<std::string> UsbSession::SyncIn(uint8_t endpoint, int length,
                                               int timeout_ms, bool interrupt) {
  ABSL_RETURN_IF_ERROR(CheckLength(length));
  ABSL_RETURN_IF_ERROR(CheckTimeout(timeout_ms));
  if ((endpoint & 0x80) == 0) {
    return absl::InvalidArgumentError("IN endpoint required");
  }
  const std::lock_guard<std::mutex> lock(impl_->pump_mutex);
  if (impl_->closed || impl_->closing) {
    return absl::FailedPreconditionError("USB session closed");
  }
  std::string data(static_cast<size_t>(length), '\0');
  int received = 0;
  if (const int result_code =
          interrupt ? libusb_interrupt_transfer(
                          impl_->handle, endpoint,
                          reinterpret_cast<unsigned char*>(data.data()), length,
                          &received, static_cast<unsigned int>(timeout_ms))
                    : libusb_bulk_transfer(
                          impl_->handle, endpoint,
                          reinterpret_cast<unsigned char*>(data.data()), length,
                          &received, static_cast<unsigned int>(timeout_ms));
      result_code != 0) {
    return TransferStatus(result_code, "USB IN transfer");
  }
  data.resize(static_cast<size_t>(received));
  return data;
}

absl::Status UsbSession::SyncOut(uint8_t endpoint, const std::string& data,
                                 int timeout_ms, bool interrupt) {
  ABSL_RETURN_IF_ERROR(CheckLength(static_cast<int>(data.size())));
  ABSL_RETURN_IF_ERROR(CheckTimeout(timeout_ms));
  if ((endpoint & 0x80) != 0) {
    return absl::InvalidArgumentError("OUT endpoint required");
  }
  const std::lock_guard<std::mutex> lock(impl_->pump_mutex);
  if (impl_->closed || impl_->closing) {
    return absl::FailedPreconditionError("USB session closed");
  }
  int sent = 0;
  if (const int result_code =
          interrupt
              ? libusb_interrupt_transfer(impl_->handle, endpoint,
                                          reinterpret_cast<unsigned char*>(
                                              const_cast<char*>(data.data())),
                                          static_cast<int>(data.size()), &sent,
                                          static_cast<unsigned int>(timeout_ms))
              : libusb_bulk_transfer(impl_->handle, endpoint,
                                     reinterpret_cast<unsigned char*>(
                                         const_cast<char*>(data.data())),
                                     static_cast<int>(data.size()), &sent,
                                     static_cast<unsigned int>(timeout_ms));
      result_code != 0) {
    return TransferStatus(result_code, "USB OUT transfer");
  }
  if (sent != static_cast<int>(data.size())) {
    return absl::DataLossError("Short USB OUT transfer");
  }
  return absl::OkStatus();
}

absl::StatusOr<std::string> UsbSession::BulkIn(uint8_t endpoints, int n,
                                               int timeout) {
  return SyncIn(endpoints, n, timeout, false);
}

absl::Status UsbSession::BulkOut(uint8_t endpoints, const std::string& data,
                                 int timeout) {
  return SyncOut(endpoints, data, timeout, false);
}

absl::StatusOr<std::string> UsbSession::InterruptIn(uint8_t endpoints, int n,
                                                    int timeout) {
  return SyncIn(endpoints, n, timeout, true);
}

absl::Status UsbSession::InterruptOut(uint8_t endpoints,
                                      const std::string& data, int timeout) {
  return SyncOut(endpoints, data, timeout, true);
}

absl::StatusOr<std::string> UsbSession::ControlIn(uint8_t request_type,
                                                  uint8_t request,
                                                  uint16_t value,
                                                  uint16_t index, int length,
                                                  int timeout_ms) {
  ABSL_RETURN_IF_ERROR(CheckLength(length));
  if (length > 65535) {
    return absl::InvalidArgumentError("Control payload too large");
  }
  ABSL_RETURN_IF_ERROR(CheckTimeout(timeout_ms));
  if ((request_type & 0x80) == 0) {
    return absl::InvalidArgumentError("IN request type required");
  }
  const std::lock_guard<std::mutex> lock(impl_->pump_mutex);
  if (impl_->closed || impl_->closing) {
    return absl::FailedPreconditionError("USB session closed");
  }
  std::string data(static_cast<size_t>(length), '\0');
  const int received = libusb_control_transfer(
      impl_->handle, request_type, request, value, index,
      reinterpret_cast<unsigned char*>(data.data()),
      static_cast<uint16_t>(length), static_cast<unsigned int>(timeout_ms));
  if (received < 0) {
    return TransferStatus(received, "USB control IN");
  }
  data.resize(static_cast<size_t>(received));
  return data;
}

absl::Status UsbSession::ControlOut(uint8_t request_type, uint8_t request,
                                    uint16_t value, uint16_t index,
                                    const std::string& data, int timeout_ms) {
  if (data.size() > 65535) {
    return absl::InvalidArgumentError("Control payload too large");
  }
  ABSL_RETURN_IF_ERROR(CheckTimeout(timeout_ms));
  if ((request_type & 0x80) != 0) {
    return absl::InvalidArgumentError("OUT request type required");
  }
  const std::lock_guard<std::mutex> lock(impl_->pump_mutex);
  if (impl_->closed || impl_->closing) {
    return absl::FailedPreconditionError("USB session closed");
  }
  const int sent = libusb_control_transfer(
      impl_->handle, request_type, request, value, index,
      reinterpret_cast<unsigned char*>(const_cast<char*>(data.data())),
      static_cast<uint16_t>(data.size()),
      static_cast<unsigned int>(timeout_ms));
  if (sent < 0) {
    return TransferStatus(sent, "USB control OUT");
  }
  if (sent != static_cast<int>(data.size())) {
    return absl::DataLossError("Short USB control OUT");
  }
  return absl::OkStatus();
}

absl::StatusOr<uint64_t> UsbSession::Submit(
    uint8_t endpoint, const std::string& data, int length, int timeout_ms,
    bool interrupt,
    std::shared_ptr<symbian::concurrency::Promise<UsbCompletion>>
        confirmation) {
  ABSL_RETURN_IF_ERROR(CheckTimeout(timeout_ms));
  const bool inbound = (endpoint & 0x80) != 0;
  if (inbound) {
    ABSL_RETURN_IF_ERROR(CheckLength(length));
  } else {
    ABSL_RETURN_IF_ERROR(CheckLength(static_cast<int>(data.size())));
  }
  const std::lock_guard<std::mutex> pump_lock(impl_->pump_mutex);
  if (impl_->closed || impl_->closing) {
    return absl::FailedPreconditionError("USB session closed");
  }
  {
    const std::lock_guard<std::mutex> state_lock(impl_->state_mutex);
    if (impl_->pending.size() + impl_->completed.size() +
            impl_->ready_futures.size() >=
        32) {
      return absl::ResourceExhaustedError(
          "USB transfer window full; drain completions");
    }
  }
  auto pending = std::make_unique<Impl::Pending>();
  pending->owner = impl_.get();
  pending->id = impl_->next_id++;
  pending->inbound = inbound;
  pending->confirmation = std::move(confirmation);
  pending->bytes.resize(inbound ? static_cast<size_t>(length) : data.size());
  if (!inbound) {
    std::memcpy(pending->bytes.data(), data.data(), data.size());
  }
  pending->transfer = libusb_alloc_transfer(0);
  if (pending->transfer == nullptr) {
    return absl::ResourceExhaustedError("Cannot allocate USB transfer");
  }
  if (interrupt) {
    libusb_fill_interrupt_transfer(
        pending->transfer, impl_->handle, endpoint, pending->bytes.data(),
        static_cast<int>(pending->bytes.size()), Impl::Callback, pending.get(),
        static_cast<unsigned int>(timeout_ms));
  } else {
    libusb_fill_bulk_transfer(
        pending->transfer, impl_->handle, endpoint, pending->bytes.data(),
        static_cast<int>(pending->bytes.size()), Impl::Callback, pending.get(),
        static_cast<unsigned int>(timeout_ms));
  }
  const uint64_t id = pending->id;
  {
    const std::lock_guard<std::mutex> state_lock(impl_->state_mutex);
    impl_->pending[id] = std::move(pending);
    if (const int result_code =
            libusb_submit_transfer(impl_->pending[id]->transfer);
        result_code != 0) {
      impl_->pending.erase(id);
      return TransferStatus(result_code, "submit USB transfer");
    }
  }
  return id;
}

absl::StatusOr<uint64_t> UsbSession::SubmitBulk(uint8_t endpoints,
                                                const std::string& data,
                                                int length, int timeout_ms) {
  return Submit(endpoints, data, length, timeout_ms, false);
}

absl::StatusOr<uint64_t> UsbSession::SubmitInterrupt(uint8_t endpoints,
                                                     const std::string& data,
                                                     int length,
                                                     int timeout_ms) {
  return Submit(endpoints, data, length, timeout_ms, true);
}

absl::StatusOr<uint64_t> UsbSession::SubmitControl(
    uint8_t request_type, uint8_t request, uint16_t value, uint16_t index,
    const std::string& data, int length, int timeout_ms,
    std::shared_ptr<symbian::concurrency::Promise<UsbCompletion>>
        confirmation) {
  ABSL_RETURN_IF_ERROR(CheckTimeout(timeout_ms));
  const bool inbound = (request_type & 0x80) != 0;
  if (inbound) {
    if (length > 65535) {
      return absl::InvalidArgumentError("Control payload too large");
    }
    ABSL_RETURN_IF_ERROR(CheckLength(length));
  } else if (data.size() > 65535) {
    return absl::InvalidArgumentError("Control payload too large");
  }
  const std::lock_guard<std::mutex> pump_lock(impl_->pump_mutex);
  if (impl_->closed || impl_->closing) {
    return absl::FailedPreconditionError("USB session closed");
  }
  {
    const std::lock_guard<std::mutex> state_lock(impl_->state_mutex);
    if (impl_->pending.size() + impl_->completed.size() +
            impl_->ready_futures.size() >=
        32) {
      return absl::ResourceExhaustedError(
          "USB transfer window full; drain completions");
    }
  }
  auto pending = std::make_unique<Impl::Pending>();
  pending->owner = impl_.get();
  pending->id = impl_->next_id++;
  pending->inbound = inbound;
  pending->control = true;
  pending->confirmation = std::move(confirmation);
  const size_t payload_size =
      inbound ? static_cast<size_t>(length) : data.size();
  pending->bytes.resize(LIBUSB_CONTROL_SETUP_SIZE + payload_size);
  libusb_fill_control_setup(pending->bytes.data(), request_type, request, value,
                            index, static_cast<uint16_t>(payload_size));
  if (!inbound) {
    std::memcpy(pending->bytes.data() + LIBUSB_CONTROL_SETUP_SIZE, data.data(),
                data.size());
  }
  pending->transfer = libusb_alloc_transfer(0);
  if (pending->transfer == nullptr) {
    return absl::ResourceExhaustedError("Cannot allocate USB transfer");
  }
  libusb_fill_control_transfer(
      pending->transfer, impl_->handle, pending->bytes.data(), Impl::Callback,
      pending.get(), static_cast<unsigned int>(timeout_ms));
  const uint64_t id = pending->id;
  {
    const std::lock_guard<std::mutex> state_lock(impl_->state_mutex);
    impl_->pending[id] = std::move(pending);
    if (const int result_code =
            libusb_submit_transfer(impl_->pending[id]->transfer);
        result_code != 0) {
      impl_->pending.erase(id);
      return TransferStatus(result_code, "submit USB control");
    }
  }
  return id;
}

absl::StatusOr<symbian::concurrency::Future<UsbCompletion>>
UsbSession::SubmitBulkFuture(uint8_t endpoint, const std::string& data,
                             int length, int timeout_ms) {
  const auto confirmation =
      std::make_shared<symbian::concurrency::Promise<UsbCompletion>>();
  symbian::concurrency::Future<UsbCompletion> future = confirmation->future();
  auto submitted =
      Submit(endpoint, data, length, timeout_ms, false, confirmation);
  ABSL_RETURN_IF_ERROR(submitted.status());
  confirmation->SetCancellationCallback(
      [session = weak_from_this(), id = *submitted] {
        if (const auto active = session.lock()) {
          active->Cancel(id).IgnoreError();
        }
      });
  return future;
}

absl::StatusOr<symbian::concurrency::Future<UsbCompletion>>
UsbSession::SubmitInterruptFuture(uint8_t endpoint, const std::string& data,
                                  int length, int timeout_ms) {
  const auto confirmation =
      std::make_shared<symbian::concurrency::Promise<UsbCompletion>>();
  symbian::concurrency::Future<UsbCompletion> future = confirmation->future();
  auto submitted =
      Submit(endpoint, data, length, timeout_ms, true, confirmation);
  ABSL_RETURN_IF_ERROR(submitted.status());
  confirmation->SetCancellationCallback(
      [session = weak_from_this(), id = *submitted] {
        if (const auto active = session.lock()) {
          active->Cancel(id).IgnoreError();
        }
      });
  return future;
}

absl::StatusOr<symbian::concurrency::Future<UsbCompletion>>
UsbSession::SubmitControlFuture(uint8_t request_type, uint8_t request,
                                uint16_t value, uint16_t index,
                                const std::string& data, int length,
                                int timeout_ms) {
  const auto confirmation =
      std::make_shared<symbian::concurrency::Promise<UsbCompletion>>();
  symbian::concurrency::Future<UsbCompletion> future = confirmation->future();
  ABSL_ASSIGN_OR_RETURN(auto submitted,
                        SubmitControl(request_type, request, value, index, data,
                                      length, timeout_ms, confirmation));
  confirmation->SetCancellationCallback(
      [session = weak_from_this(), id = submitted] {
        if (const auto active = session.lock()) {
          active->Cancel(id).IgnoreError();
        }
      });
  return future;
}

absl::Status UsbSession::Cancel(uint64_t id) {
  const std::lock_guard<std::mutex> pump_lock(impl_->pump_mutex);
  if (impl_->closed || impl_->closing) {
    return absl::FailedPreconditionError("USB session closed");
  }
  const std::lock_guard<std::mutex> state_lock(impl_->state_mutex);
  const auto it = impl_->pending.find(id);
  if (it == impl_->pending.end()) {
    return absl::NotFoundError("USB transfer not pending");
  }
  return TransferStatus(libusb_cancel_transfer(it->second->transfer),
                        "cancel USB transfer");
}

absl::StatusOr<std::vector<UsbCompletion>> UsbSession::HandleEvents(
    int timeout_ms) {
  if (timeout_ms < 0 || timeout_ms > 5000) {
    return absl::InvalidArgumentError("USB event wait must be 0..5000 ms");
  }
  const std::lock_guard<std::mutex> pump_lock(impl_->pump_mutex);
  if (impl_->closed || impl_->closing) {
    return absl::FailedPreconditionError("USB session closed");
  }
  const bool future_settled = impl_->SettleReadyFutures();
  {
    const std::lock_guard<std::mutex> state_lock(impl_->state_mutex);
    if (!impl_->completed.empty()) {
      std::vector<UsbCompletion> ready;
      ready.swap(impl_->completed);
      return ready;
    }
  }
  if (future_settled) {
    return std::vector<UsbCompletion>{};
  }
  timeval wait{.tv_sec = timeout_ms / 1000,
               .tv_usec = (timeout_ms % 1000) * 1000};
  if (const int result_code = libusb_handle_events_timeout_completed(
          impl_->context, &wait, nullptr);
      result_code != 0 && result_code != LIBUSB_ERROR_INTERRUPTED) {
    return TransferStatus(result_code, "handle USB events");
  }
  impl_->SettleReadyFutures();
  const std::lock_guard<std::mutex> state_lock(impl_->state_mutex);
  std::vector<UsbCompletion> result;
  result.swap(impl_->completed);
  return result;
}

absl::StatusOr<std::vector<UsbPollFd>> UsbSession::PollFileDescriptors() const {
  const std::lock_guard<std::mutex> pump_lock(impl_->pump_mutex);
  if (impl_->closed || impl_->closing) {
    return absl::FailedPreconditionError("USB session closed");
  }
  const libusb_pollfd* absl_nullable* absl_nullable list =
      libusb_get_pollfds(impl_->context);
  if (list == nullptr) {
    return absl::UnimplementedError("libusb poll descriptors unavailable");
  }
  std::vector<UsbPollFd> result;
  for (size_t i = 0; list[i] != nullptr && i < 1024; ++i) {
    result.push_back({list[i]->fd, list[i]->events});
  }
  libusb_free_pollfds(list);
  return result;
}

absl::StatusOr<int> UsbSession::NextTimeoutMs() const {
  const std::lock_guard<std::mutex> pump_lock(impl_->pump_mutex);
  if (impl_->closed || impl_->closing) {
    return absl::FailedPreconditionError("USB session closed");
  }
  timeval wait{};
  const int result_code = libusb_get_next_timeout(impl_->context, &wait);
  if (result_code < 0) {
    return TransferStatus(result_code, "next USB timeout");
  }
  if (result_code == 0) {
    return -1;
  }
  return static_cast<int>(wait.tv_sec * 1000 + wait.tv_usec / 1000);
}

absl::Status UsbSession::Close() {
  const std::lock_guard<std::mutex> pump_lock(impl_->pump_mutex);
  if (impl_->closed) {
    return absl::OkStatus();
  }
  impl_->closing = true;
  {
    const std::lock_guard<std::mutex> state_lock(impl_->state_mutex);
    for (auto& [id, pending] : impl_->pending) {
      libusb_cancel_transfer(pending->transfer);
    }
  }
  for (int i = 0; i < 40; ++i) {
    {
      const std::lock_guard<std::mutex> state_lock(impl_->state_mutex);
      if (impl_->pending.empty()) {
        break;
      }
    }
    timeval wait{.tv_sec = 0, .tv_usec = 50000};
    libusb_handle_events_timeout_completed(impl_->context, &wait, nullptr);
  }
  {
    const std::lock_guard<std::mutex> state_lock(impl_->state_mutex);
    if (!impl_->pending.empty()) {
      return absl::UnavailableError(
          "USB transfers did not cancel; retry close");
    }
  }
  impl_->SettleReadyFutures();
  for (auto& [number, alternate] : impl_->claimed) {
    if (alternate != 0) {
      libusb_set_interface_alt_setting(impl_->handle, number, 0);
    }
    libusb_release_interface(impl_->handle, number);
  }
  impl_->claimed.clear();
  libusb_free_config_descriptor(impl_->config);
  libusb_close(impl_->handle);
  libusb_exit(impl_->context);
  impl_->config = nullptr;
  impl_->handle = nullptr;
  impl_->context = nullptr;
  impl_->closed = true;
  return absl::OkStatus();
}

absl::StatusOr<std::vector<UsbDeviceDescriptor>> ListUsbDevices() {
  Context context;
  if (const int result_code = libusb_init(&context.value); result_code != 0) {
    return TransferStatus(result_code, "libusb init");
  }
  DeviceList list;
  const ssize_t count = libusb_get_device_list(context.value, &list.value);
  if (count < 0) {
    return TransferStatus(static_cast<int>(count), "libusb list");
  }
  std::vector<UsbDeviceDescriptor> result;
  for (ssize_t i = 0; i < count; ++i) {
    libusb_device_descriptor descriptor{};
    if (libusb_get_device_descriptor(list.value[i], &descriptor) != 0) {
      continue;
    }
    unsigned char ports[8];
    const int port_count =
        libusb_get_port_numbers(list.value[i], ports, sizeof(ports));
    UsbDeviceDescriptor item;
    item.vendor_id = descriptor.idVendor;
    item.product_id = descriptor.idProduct;
    item.bus = libusb_get_bus_number(list.value[i]);
    item.address = libusb_get_device_address(list.value[i]);
    item.device_class = descriptor.bDeviceClass;
    item.configuration_count = descriptor.bNumConfigurations;
    for (int j = 0; j < port_count; ++j) {
      item.ports.push_back(ports[j]);
    }
    result.push_back(std::move(item));
  }
  return result;
}
}  // namespace symbian::device
