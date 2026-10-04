#ifndef SYMBIAN_DEVICE_TRANSPORT_H_
#define SYMBIAN_DEVICE_TRANSPORT_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <absl/status/status.h>
#include <absl/status/statusor.h>

#include "symbian/concurrency/future.h"
#include "symbian/device/usb.h"

namespace symbian::device {

struct UsbPollFd {
  // Host file descriptor watched by libusb.
  int fd = -1;
  // Poll event bitmask requested by libusb.
  int events = 0;
};

struct UsbCompletion {
  // Transfer identifier for the explicit submission API.
  uint64_t id = 0;
  // Libusb completion state.
  std::string status;
  // Number of bytes transferred.
  int actual_length = 0;
  // Received data for inbound transfers; empty for outbound transfers.
  std::string data;
};

class UsbSession : public std::enable_shared_from_this<UsbSession> {
 public:
  static absl::StatusOr<std::shared_ptr<UsbSession>> Open(
      uint16_t vendor, uint16_t product, const std::string& serial_anchor);
  ~UsbSession();
  UsbSession(const UsbSession&) = delete;
  UsbSession& operator=(const UsbSession&) = delete;

  absl::StatusOr<UsbProbe> Descriptors() const;
  absl::Status Claim(int number);
  absl::Status SetAlternate(int number, int alternate);
  absl::Status Release(int number);
  absl::StatusOr<std::string> BulkIn(uint8_t endpoint, int length,
                                     int timeout_ms);
  absl::Status BulkOut(uint8_t endpoint, const std::string& data,
                       int timeout_ms);
  absl::StatusOr<std::string> InterruptIn(uint8_t endpoint, int length,
                                          int timeout_ms);
  absl::Status InterruptOut(uint8_t endpoint, const std::string& data,
                            int timeout_ms);
  absl::StatusOr<std::string> ControlIn(uint8_t request_type, uint8_t request,
                                        uint16_t value, uint16_t index,
                                        int length, int timeout_ms);
  absl::Status ControlOut(uint8_t request_type, uint8_t request, uint16_t value,
                          uint16_t index, const std::string& data,
                          int timeout_ms);
  absl::StatusOr<uint64_t> SubmitBulk(uint8_t endpoint, const std::string& data,
                                      int length, int timeout_ms);
  absl::StatusOr<uint64_t> SubmitInterrupt(uint8_t endpoint,
                                           const std::string& data, int length,
                                           int timeout_ms);
  absl::StatusOr<uint64_t> SubmitControl(
      uint8_t request_type, uint8_t request, uint16_t value, uint16_t index,
      const std::string& data, int length, int timeout_ms,
      std::shared_ptr<symbian::concurrency::Promise<UsbCompletion>>
          confirmation = nullptr);
  absl::StatusOr<symbian::concurrency::Future<UsbCompletion>> SubmitBulkFuture(
      uint8_t endpoint, const std::string& data, int length, int timeout_ms);
  absl::StatusOr<symbian::concurrency::Future<UsbCompletion>>
  SubmitInterruptFuture(uint8_t endpoint, const std::string& data, int length,
                        int timeout_ms);
  absl::StatusOr<symbian::concurrency::Future<UsbCompletion>>
  SubmitControlFuture(uint8_t request_type, uint8_t request, uint16_t value,
                      uint16_t index, const std::string& data, int length,
                      int timeout_ms);
  absl::Status Cancel(uint64_t id);
  absl::StatusOr<std::vector<UsbCompletion>> HandleEvents(int timeout_ms);
  absl::StatusOr<std::vector<UsbPollFd>> PollFileDescriptors() const;
  absl::StatusOr<int> NextTimeoutMs() const;
  absl::Status Close();

 private:
  struct Impl;
  explicit UsbSession(std::unique_ptr<Impl> impl);
  absl::StatusOr<std::string> SyncIn(uint8_t endpoint, int length,
                                     int timeout_ms, bool interrupt);
  absl::Status SyncOut(uint8_t endpoint, const std::string& data,
                       int timeout_ms, bool interrupt);
  absl::StatusOr<uint64_t> Submit(
      uint8_t endpoint, const std::string& data, int length, int timeout_ms,
      bool interrupt,
      std::shared_ptr<symbian::concurrency::Promise<UsbCompletion>>
          confirmation = nullptr);
  std::unique_ptr<Impl> impl_;
};

absl::StatusOr<std::vector<UsbDeviceDescriptor>> ListUsbDevices();

}  // namespace symbian::device
#endif  // SYMBIAN_DEVICE_TRANSPORT_H_
