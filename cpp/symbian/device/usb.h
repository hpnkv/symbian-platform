#ifndef SYMBIAN_DEVICE_USB_H_
#define SYMBIAN_DEVICE_USB_H_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <absl/status/statusor.h>

namespace symbian::device {

struct UsbEndpointDescriptor {
  // USB endpoint address, including its direction bit.
  int address = 0;
  // Human-readable transfer direction.
  std::string direction;
  // USB transfer type.
  std::string transfer;
  // Maximum packet size reported by the active descriptor.
  int max_packet_bytes = 0;
};

struct UsbInterfaceDescriptor {
  // Interface number within the active configuration.
  int number = 0;
  // Active alternate setting described by this record.
  int alternate_setting = 0;
  // USB class, subclass, and protocol codes.
  int class_code = 0;
  int subclass_code = 0;
  int protocol_code = 0;
  // Endpoints belonging to this alternate setting.
  std::vector<UsbEndpointDescriptor> endpoints;
};

struct UsbCdcUnion {
  // CDC control interface number.
  int master = 0;
  // CDC data interface numbers associated with the control interface.
  std::vector<int> slaves;
};

struct UsbInterfaceAssociation {
  // First interface number and number of associated interfaces.
  int first_interface = 0;
  int interface_count = 0;
  // USB function class, subclass, and protocol codes.
  int class_code = 0;
  int subclass_code = 0;
  int protocol_code = 0;
};

struct MtpDeviceInfo {
  // PTP standard and vendor extension versions.
  int standard_version = 0;
  uint32_t vendor_extension_id = 0;
  int vendor_extension_version = 0;
  std::string vendor_extension_description;
  int functional_mode = 0;
  // Supported PTP operation codes, bounded during parsing.
  std::vector<uint32_t> supported_operation_codes;
  // Device-reported identity, excluding the private serial number.
  std::string manufacturer;
  std::string model;
  std::string device_version;
};

struct MtpObjectInfo {
  // PTP object handle.
  uint32_t handle = 0;
  // Populated when the object-info request succeeds.
  std::optional<int> format_code;
  std::optional<uint32_t> size_bytes;
  std::optional<std::string> name;
  // Request or decode error, if any.
  std::optional<std::string> error;
  std::optional<int> response_code;
};

struct MtpStorageInfo {
  // PTP storage identifier.
  uint32_t id = 0;
  // Storage metadata populated on a successful request.
  std::optional<int> storage_type;
  std::optional<int> filesystem_type;
  std::optional<int> access_capability;
  std::optional<uint64_t> total_bytes;
  std::optional<uint64_t> free_bytes;
  std::optional<uint32_t> free_images;
  std::optional<std::string> description;
  std::optional<std::string> volume_label;
  // Optional bounded root listing and any partial failure.
  std::optional<size_t> root_object_count;
  std::vector<MtpObjectInfo> root_objects;
  std::optional<std::string> root_listing_error;
  std::optional<int> root_listing_response_code;
  std::optional<std::string> error;
  std::optional<int> response_code;
};

struct UsbProbe {
  // Probe state and backend. Unknown device details remain absent.
  std::string state = "device-unavailable";
  std::string backend = "libusb-static";
  std::optional<std::string> detail;
  std::optional<int> response_code;
  std::string scope;
  // Descriptor map fields.
  std::optional<std::string> identity_basis;
  std::optional<int> configuration;
  std::vector<UsbInterfaceDescriptor> interfaces;
  std::vector<UsbCdcUnion> cdc_unions;
  std::vector<UsbInterfaceAssociation> interface_associations;
  // MTP fields.
  std::optional<std::string> transport;
  std::optional<int> interface_number;
  std::optional<MtpDeviceInfo> device_info;
  std::vector<MtpStorageInfo> storage;
  std::optional<size_t> storage_count;
  std::optional<bool> session_closed;
  // OBEX connect/disconnect fields.
  std::optional<std::string> target;
  std::optional<bool> connection_id_present;
  std::optional<bool> disconnected;
  std::optional<std::string> disconnect_error;
  std::optional<int> disconnect_response_code;
  std::optional<bool> alternate_restored;
  std::optional<bool> interface_released;
  std::optional<int> response_bytes;
};

struct UsbDeviceDescriptor {
  // Public USB vendor and product identifiers.
  int vendor_id = 0;
  int product_id = 0;
  // Host bus address and port path; no serial is exposed.
  int bus = 0;
  int address = 0;
  std::vector<int> ports;
  // USB device class and configuration count.
  int device_class = 0;
  int configuration_count = 0;
};

/** Result of a checked SIS transfer to an MTP Installs folder. */
struct MtpStageResult {
  /// Selected writable store reported by the MTP device.
  uint32_t storage_id = 0;
  /// New or already matching object, usable for later read-only inspection.
  uint32_t object_handle = 0;
  /// Content-addressed SIS name under the store's root Installs folder.
  std::string name;
  /// False when an existing object passed the same readback digest check.
  bool copied = false;
};

/** Inspect a serial-matched USB device without changing its stored files.
 *
 * `operation` is `map`, `mtp` or `obex`. `mtp` may return a bounded root
 * listing; `obex` performs a Connect/Disconnect probe. No raw serial is
 * returned. The caller must check `UsbProbe::state` for protocol outcomes.
 */
absl::StatusOr<UsbProbe> InspectUsb(uint16_t vendor, uint16_t product,
                                    const std::string& anchor,
                                    const std::string& operation,
                                    uint32_t limit);

/** Stage one SIS on a serial-matched device, checking the readback digest.
 *
 * The device installer is not invoked. The transfer requires one writable MTP
 * store with an unambiguous root Installs folder. Files larger than 16 MiB are
 * rejected. The supplied SHA-256 binds the package to the caller's inspection.
 */
absl::StatusOr<MtpStageResult> StageMtpSis(uint16_t vendor, uint16_t product,
                                           const std::string& anchor,
                                           const std::string& package_path,
                                           const std::string& filename,
                                           const std::string& expected_sha256);

}  // namespace symbian::device
#endif  // SYMBIAN_DEVICE_USB_H_
