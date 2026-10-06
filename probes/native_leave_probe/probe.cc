#include <absl/base/nullability.h>
#include <e32base.h>

#ifndef SYMBIAN_LEAVE_EXPECTED
#define SYMBIAN_LEAVE_EXPECTED 32
#endif

namespace {

class Scope final {
 public:
  explicit Scope(TInt* absl_nonnull destroyed) : destroyed_(destroyed) {}

  ~Scope() { ++*destroyed_; }

 private:
  TInt* absl_nonnull destroyed_;
};

void InnerL(TInt* absl_nonnull destroyed) {
  Scope scope(destroyed);
  User::Leave(KErrArgument);
}

void OuterL(TInt* absl_nonnull destroyed) {
  Scope scope(destroyed);
  TRAPD(inner, InnerL(destroyed));
  if (inner != KErrArgument) {
    User::Leave(KErrCorrupt);
  }
  User::Leave(KErrNotFound);
}

}  // namespace

int main() {
  TInt destroyed = 0;
  const TInt cells = User::CountAllocCells();
  for (TInt attempt = 0; attempt < 16; ++attempt) {
    TRAPD(error, OuterL(&destroyed));
    if (error != KErrNotFound) {
      return -401;
    }
  }
  if (destroyed != SYMBIAN_LEAVE_EXPECTED) {
    return -402;
  }
  // The SDK's bounded executable heap is at most one MiB.
  TRAPD(oom, User::Free(User::AllocL(2 * 1024 * 1024)));
  if (oom != KErrNoMemory) {
    return -403;
  }
  if (User::CountAllocCells() != cells) {
    return -404;
  }
  return KErrNone;
}
