// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "symbian/api/system/application_management.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <type_traits>

#include <absl/base/nullability.h>
#include <apgcli.h>

#include "symbian/native_status.h"

namespace symbian::api::system {
namespace {

// Resolve AppArc when used so a missing export cannot prevent process startup.
constexpr int kAppArcConnectOrdinal = 25;
constexpr int kAppArcSessionConstructorOrdinal = 28;
constexpr int kAppArcStartDocumentOrdinal = 20;
constexpr int kAppArcGetAppInfoOrdinal = 147;
constexpr int kAppArcCloseOrdinal = 323;
constexpr int kAppInfoConstructorOrdinal = 22;

using SessionConstructor = void(RApaLsSession* absl_nonnull);
using SessionConnect = TInt(RApaLsSession* absl_nonnull);
using SessionClose = void(RApaLsSession* absl_nonnull);
using StartDocumentFunction = TInt(RApaLsSession* absl_nonnull, const TDesC&,
                                   TThreadId&, RApaLsSession::TLaunchType);
using GetAppInfoFunction =
    TInt(const RApaLsSession* absl_nonnull, TApaAppInfo&, TUid);
using AppInfoConstructor = void(TApaAppInfo* absl_nonnull);

class LoadedLibrary {
 public:
  explicit LoadedLibrary(const TDesC& name) : result_(library_.Load(name)) {}

  LoadedLibrary(const LoadedLibrary&) = delete;
  LoadedLibrary& operator=(const LoadedLibrary&) = delete;

  ~LoadedLibrary() {
    if (result_ == KErrNone) {
      library_.Close();
    }
  }

  absl::Status status() const {
    return symbian::StatusFromNativeError(result_, "AppArc library");
  }

  template <typename Signature>
  absl::StatusOr<std::function<Signature>> Lookup(int ordinal) const {
    if (result_ != KErrNone) {
      return status();
    }
    TLibraryFunction function = library_.Lookup(ordinal);
    if (function == nullptr) {
      return absl::UnimplementedError("AppArc export unavailable on device");
    }
    // RLibrary exposes an ordinal as a C ABI procedure address. Keep the
    // conversion here; all callers receive an ordinary SDK callable.
    return std::function<Signature>(
        reinterpret_cast<std::add_pointer_t<Signature>>(function));
  }

 private:
  RLibrary library_;
  TInt result_;
};

class AppArcSession {
 public:
  static absl::StatusOr<std::unique_ptr<AppArcSession>> Open(
      const LoadedLibrary& library) {
    auto constructor =
        library.Lookup<SessionConstructor>(kAppArcSessionConstructorOrdinal);
    auto connect = library.Lookup<SessionConnect>(kAppArcConnectOrdinal);
    auto close = library.Lookup<SessionClose>(kAppArcCloseOrdinal);
    if (!constructor.ok()) {
      return constructor.status();
    }
    if (!connect.ok()) {
      return connect.status();
    }
    if (!close.ok()) {
      return close.status();
    }
    auto session =
        std::unique_ptr<AppArcSession>(new AppArcSession(std::move(*close)));
    (*constructor)(session->native());
    session->constructed_ = true;
    const TInt result = (*connect)(session->native());
    if (result != KErrNone) {
      return symbian::StatusFromNativeError(result, "AppArc session");
    }
    return session;
  }

  AppArcSession(const AppArcSession&) = delete;
  AppArcSession& operator=(const AppArcSession&) = delete;

  ~AppArcSession() {
    if (constructed_) {
      close_(native());
      std::destroy_at(native());
    }
  }

  RApaLsSession* absl_nonnull native() {
    return reinterpret_cast<RApaLsSession*>(storage_);
  }

 private:
  explicit AppArcSession(std::function<SessionClose> close)
      : close_(std::move(close)) {}

  alignas(RApaLsSession) std::byte storage_[sizeof(RApaLsSession)]{};
  std::function<SessionClose> close_;
  bool constructed_ = false;
};

}  // namespace

absl::Status OpenDocument(std::u16string_view absolute_path) {
  if (absolute_path.empty() || absolute_path.size() > 1024 ||
      absolute_path.find(u'\0') != std::u16string_view::npos) {
    return absl::InvalidArgumentError("Invalid document path");
  }
  _LIT(KAppArcLibrary, "apgrfx.dll");
  LoadedLibrary library(KAppArcLibrary);
  if (!library.status().ok()) {
    return library.status();
  }
  auto start =
      library.Lookup<StartDocumentFunction>(kAppArcStartDocumentOrdinal);
  if (!start.ok()) {
    return start.status();
  }
  auto session = AppArcSession::Open(library);
  if (!session.ok()) {
    return session.status();
  }
  const TPtrC path(reinterpret_cast<const TUint16*>(absolute_path.data()),
                   static_cast<TInt>(absolute_path.size()));
  TThreadId launched;
  const TInt result = (*start)((*session)->native(), path, launched,
                               RApaLsSession::ELaunchNewApp);
  return symbian::StatusFromNativeError(result, "open document");
}

absl::StatusOr<bool> IsApplicationRegistered(std::uint32_t uid) {
  if (uid == 0) {
    return absl::InvalidArgumentError("Invalid application UID");
  }
  _LIT(KAppArcLibrary, "apgrfx.dll");
  _LIT(KAppInfoLibrary, "apparc.dll");
  LoadedLibrary app_arc(KAppArcLibrary);
  LoadedLibrary app_info(KAppInfoLibrary);
  if (!app_arc.status().ok()) {
    return app_arc.status();
  }
  if (!app_info.status().ok()) {
    return app_info.status();
  }
  auto get_info = app_arc.Lookup<GetAppInfoFunction>(kAppArcGetAppInfoOrdinal);
  auto constructor =
      app_info.Lookup<AppInfoConstructor>(kAppInfoConstructorOrdinal);
  if (!get_info.ok()) {
    return get_info.status();
  }
  if (!constructor.ok()) {
    return constructor.status();
  }
  auto session = AppArcSession::Open(app_arc);
  if (!session.ok()) {
    return session.status();
  }
  alignas(TApaAppInfo) std::byte storage[sizeof(TApaAppInfo)]{};
  auto* absl_nonnull info = reinterpret_cast<TApaAppInfo*>(storage);
  (*constructor)(info);
  const TInt result = (*get_info)((*session)->native(), *info, TUid::Uid(uid));
  std::destroy_at(info);
  if (result == KErrNotFound) {
    return false;
  }
  if (result != KErrNone) {
    return symbian::StatusFromNativeError(result, "application registration");
  }
  return true;
}

}  // namespace symbian::api::system
