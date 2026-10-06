#include <absl/base/nullability.h>

// The SDK DLL entry constructs this object when EUSER attaches the module.
// The simple event value can be read through the frozen function export.
extern "C" __attribute__((visibility("hidden"))) int SymbianLifecycleEvents = 0;
extern "C" __attribute__((visibility(
    "hidden"))) volatile int* absl_nullable SymbianLifecycleSink = nullptr;

class LifecycleObject {
 public:
  LifecycleObject() { SymbianLifecycleEvents = 12; }

  ~LifecycleObject() {
    SymbianLifecycleEvents = 34;
    if (SymbianLifecycleSink != nullptr) {
      *SymbianLifecycleSink = 34;
    }
  }
};

__attribute__((visibility("hidden"))) LifecycleObject SymbianLifecycleObject;

extern "C" __attribute__((visibility("default"))) int SymbianLifecycleState() {
  return SymbianLifecycleEvents;
}

extern "C" __attribute__((visibility("default"))) void SymbianLifecycleSetSink(
    volatile int* absl_nonnull sink) {
  SymbianLifecycleSink = sink;
}
