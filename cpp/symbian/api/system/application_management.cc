// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "symbian/api/system/application_management.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <type_traits>

#include <absl/status/status_macros.h>
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
using GetAppInfoFunction = TInt(const RApaLsSession* absl_nonnull, TApaAppInfo&,
                                TUid);
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
    ABSL_ASSIGN_OR_RETURN(
        auto constructor,
        library.Lookup<SessionConstructor>(kAppArcSessionConstructorOrdinal));
    ABSL_ASSIGN_OR_RETURN(
        auto connect, library.Lookup<SessionConnect>(kAppArcConnectOrdinal));
    ABSL_ASSIGN_OR_RETURN(
        auto close, library.Lookup<SessionClose>(kAppArcCloseOrdinal));
    auto session =
        std::unique_ptr<AppArcSession>(new AppArcSession(std::move(close)));
    constructor(session->native());
    session->constructed_ = true;
    if (const TInt result = connect(session->native()); result != KErrNone) {
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
  const LoadedLibrary library(KAppArcLibrary);
  ABSL_RETURN_IF_ERROR(library.status());
  ABSL_ASSIGN_OR_RETURN(
      auto start,
      library.Lookup<StartDocumentFunction>(kAppArcStartDocumentOrdinal));
  ABSL_ASSIGN_OR_RETURN(auto session, AppArcSession::Open(library));
  const TPtrC path(reinterpret_cast<const TUint16*>(absolute_path.data()),
                   static_cast<TInt>(absolute_path.size()));
  TThreadId launched;
  const TInt result = start(session->native(), path, launched,
                               RApaLsSession::ELaunchNewApp);
  return symbian::StatusFromNativeError(result, "open document");
}

absl::StatusOr<bool> IsApplicationRegistered(std::uint32_t uid) {
  if (uid == 0) {
    return absl::InvalidArgumentError("Invalid application UID");
  }
  _LIT(KAppArcLibrary, "apgrfx.dll");
  _LIT(KAppInfoLibrary, "apparc.dll");
  const LoadedLibrary app_arc(KAppArcLibrary);
  const LoadedLibrary app_info(KAppInfoLibrary);
  ABSL_RETURN_IF_ERROR(app_arc.status());
  ABSL_RETURN_IF_ERROR(app_info.status());
  ABSL_ASSIGN_OR_RETURN(
      auto get_info,
      app_arc.Lookup<GetAppInfoFunction>(kAppArcGetAppInfoOrdinal));
  ABSL_ASSIGN_OR_RETURN(
      auto constructor,
      app_info.Lookup<AppInfoConstructor>(kAppInfoConstructorOrdinal));
  ABSL_ASSIGN_OR_RETURN(auto session, AppArcSession::Open(app_arc));
  alignas(TApaAppInfo) std::byte storage[sizeof(TApaAppInfo)]{};
  auto* absl_nonnull info = reinterpret_cast<TApaAppInfo*>(storage);
  constructor(info);
  const TInt result = get_info(session->native(), *info, TUid::Uid(uid));
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
