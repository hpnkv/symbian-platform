// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "symbian/api/system/serial_ports.h"

#include <cstddef>
#include <functional>
#include <type_traits>

#include <absl/base/nullability.h>
#include <c32comm.h>

#include "symbian/native_status.h"

namespace symbian::api::system {
namespace {

// The C32 exports are loaded only when the inventory is requested. Some Belle
// images omit optional DLL imports and otherwise fail before app startup.
constexpr int kConstructorOrdinal = 72;
constexpr int kConnectOrdinal = 70;
constexpr int kNumPortsOrdinal = 71;
constexpr int kGetPortInfoOrdinal = 61;

template <typename Signature>
absl::StatusOr<std::function<Signature>> Lookup(const RLibrary& library,
                                                int ordinal) {
  TLibraryFunction function = library.Lookup(ordinal);
  if (function == nullptr) {
    return absl::UnimplementedError("C32 serial export unavailable");
  }
  return std::function<Signature>(
      reinterpret_cast<std::add_pointer_t<Signature>>(function));
}

std::u16string CopyText(const TDesC16& source) {
  const auto* absl_nonnull data =
      reinterpret_cast<const char16_t*>(source.Ptr());
  return std::u16string(data, static_cast<std::size_t>(source.Length()));
}

}  // namespace

absl::StatusOr<std::vector<SerialPortRange>> ListSerialPortRanges() {
  RLibrary library;
  if (const TInt loaded = library.Load(_L("c32.dll")); loaded != KErrNone) {
    return symbian::StatusFromNativeError(loaded, "C32 library");
  }

  struct LibraryCloser {
    RLibrary* absl_nonnull library;

    ~LibraryCloser() { library->Close(); }
  } closer{&library};

  auto constructor =
      Lookup<void(RCommServ* absl_nonnull)>(library, kConstructorOrdinal);
  auto connect =
      Lookup<TInt(RCommServ* absl_nonnull)>(library, kConnectOrdinal);
  auto count_ports = Lookup<TInt(RCommServ* absl_nonnull, TInt* absl_nonnull)>(
      library, kNumPortsOrdinal);
  auto port_info =
      Lookup<TInt(RCommServ* absl_nonnull, TInt, TDes16* absl_nonnull,
                  TSerialInfo* absl_nonnull)>(library, kGetPortInfoOrdinal);
  if (!constructor.ok()) {
    return constructor.status();
  }
  if (!connect.ok()) {
    return connect.status();
  }
  if (!count_ports.ok()) {
    return count_ports.status();
  }
  if (!port_info.ok()) {
    return port_info.status();
  }

  alignas(RCommServ) std::byte storage[sizeof(RCommServ)]{};
  auto* absl_nonnull session = reinterpret_cast<RCommServ*>(storage);
  (*constructor)(session);
  if (const TInt connected = (*connect)(session); connected != KErrNone) {
    return symbian::StatusFromNativeError(connected, "C32 connect");
  }

  struct SessionCloser {
    RCommServ* absl_nonnull session;

    ~SessionCloser() { session->Close(); }
  } session_closer{session};

  TInt count = 0;
  if (const TInt counted = (*count_ports)(session, &count);
      counted != KErrNone) {
    return symbian::StatusFromNativeError(counted, "C32 port count");
  }
  if (count < 0 || count > 128) {
    return absl::OutOfRangeError("C32 port count outside supported bound");
  }
  std::vector<SerialPortRange> result;
  result.reserve(static_cast<std::size_t>(count));
  for (TInt index = 0; index < count; ++index) {
    TBuf<64> module;
    TSerialInfo info{};
    if (const TInt fetched = (*port_info)(session, index, &module, &info);
        fetched != KErrNone) {
      return symbian::StatusFromNativeError(fetched, "C32 port info");
    }
    result.push_back({CopyText(module), CopyText(info.iName),
                      CopyText(info.iDescription), info.iLowUnit,
                      info.iHighUnit});
  }
  return result;
}

}  // namespace symbian::api::system
