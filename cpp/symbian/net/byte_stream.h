// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.
#ifndef SYMBIAN_NET_BYTE_STREAM_H_
#define SYMBIAN_NET_BYTE_STREAM_H_
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <utility>

#include <absl/base/nullability.h>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"

namespace symbian::net {
/** @brief Move-only owner of one TCP, TLS, or application transport.
 *
 * Read returns zero only at clean EOF. Write completes the whole span or fails.
 * Operations and destruction run on the owning worker.
 */
class ByteStream final {
 public:
  ByteStream() = default;

  template <typename Transport>
  ByteStream(std::unique_ptr<Transport> transport)
      : owner_(transport.release(), Destroy<Transport>),
        read_(ReadTransport<Transport>),
        write_(WriteTransport<Transport>),
        close_(CloseTransport<Transport>) {}

  ByteStream(ByteStream&&) noexcept = default;
  ByteStream& operator=(ByteStream&&) noexcept = default;
  ByteStream(const ByteStream&) = delete;
  ByteStream& operator=(const ByteStream&) = delete;

  explicit operator bool() const { return owner_ != nullptr; }

  absl::StatusOr<std::size_t> Read(std::span<std::uint8_t> bytes,
                                   absl::Time deadline) {
    if (owner_ == nullptr) {
      return absl::FailedPreconditionError("Byte stream is empty");
    }
    return read_(owner_.get(), bytes, deadline);
  }

  absl::Status Write(std::span<const std::uint8_t> bytes, absl::Time deadline) {
    if (owner_ == nullptr) {
      return absl::FailedPreconditionError("Byte stream is empty");
    }
    return write_(owner_.get(), bytes, deadline);
  }

  void Close() {
    if (owner_ != nullptr) {
      close_(owner_.get());
    }
  }

 private:
  template <typename Transport>
  static void Destroy(void* absl_nonnull owner) {
    delete static_cast<Transport*>(owner);
  }

  template <typename Transport>
  static absl::StatusOr<std::size_t> ReadTransport(
      void* absl_nonnull owner, std::span<std::uint8_t> bytes,
      absl::Time deadline) {
    return static_cast<Transport*>(owner)->Read(bytes, deadline);
  }

  template <typename Transport>
  static absl::Status WriteTransport(void* absl_nonnull owner,
                                     std::span<const std::uint8_t> bytes,
                                     absl::Time deadline) {
    return static_cast<Transport*>(owner)->Write(bytes, deadline);
  }

  template <typename Transport>
  static void CloseTransport(void* absl_nonnull owner) {
    static_cast<Transport*>(owner)->Close();
  }

  std::unique_ptr<void, std::function<void(void* absl_nonnull)>> owner_{
      nullptr, [](void* absl_nullable) {}};
  std::function<absl::StatusOr<std::size_t>(void* absl_nonnull,
                                            std::span<std::uint8_t>,
                                            absl::Time)> read_;
  std::function<absl::Status(void* absl_nonnull,
                             std::span<const std::uint8_t>, absl::Time)>
      write_;
  std::function<void(void* absl_nonnull)> close_;
};
}  // namespace symbian::net
#endif
