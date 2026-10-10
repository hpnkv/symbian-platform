#include <cstddef>
#include <span>
#include <string_view>

#include <absl/base/nullability.h>
#include <e32std.h>
#include <u32std.h>

#include "symbian/api/storage/storage.h"
#include "symbian/runtime.h"

extern "C" int RuntimeMain();

namespace {

void TraceStartup(std::string_view message) {
  static std::size_t offset = 0;
  auto file = symbian::api::storage::WritableFile::Open(
      u"E:\\Others\\agent_startup.txt",
      offset == 0 ? symbian::api::storage::WriteMode::kReplaceExisting
                  : symbian::api::storage::WriteMode::kOpenExisting);
  if (!file.ok()) {
    return;
  }
  const auto* absl_nonnull bytes =
      reinterpret_cast<const std::byte*>(message.data());
  if (file->WriteAt(offset, std::span(bytes, message.size())).ok() &&
      file->WriteAt(offset + message.size(),
                    std::span(reinterpret_cast<const std::byte*>("\n"), 1))
          .ok()) {
    offset += message.size() + 1;
    file->Flush().IgnoreError();
  }
}

}  // namespace

#ifdef SYMBIAN_RUNTIME_GLOBAL_LIFETIME
extern "C" int RuntimeCheckFinalizers();
#endif

extern "C" void RuntimeRunThread(TInt reason,
                                 SStdEpocThreadCreateInfo* absl_nullable info) {
  if ((reason != 0 && reason != 1) || info == nullptr) {
    User::Invariant();
    return;
  }
  const TBool secondary = reason == 1;
  TInt result = UserHeap::SetupThreadHeap(secondary, *info);
  if (result == KErrNone) {
    if (secondary) {
      result = info->iFunction(info->iPtr);
    } else {
      User::InitProcess();
      TraceStartup("User::InitProcess completed");
      SymbianRuntimeRunInitializers();
      TraceStartup("global initializers completed");
      result = RuntimeMain();
      SymbianRuntimeRunFinalizers();
#ifdef SYMBIAN_RUNTIME_GLOBAL_LIFETIME
      if (result == 0) {
        result = RuntimeCheckFinalizers();
      }
#endif
    }
  }
  User::Exit(result);
}
