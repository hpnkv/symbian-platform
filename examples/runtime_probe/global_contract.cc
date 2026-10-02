#include <string>

#include "abi.h"

extern "C" __attribute__((visibility("default"))) int RuntimeGlobalEvents = 0;
extern "C" __attribute__((visibility("default"))) int RuntimeGlobalCells = 0;

class GlobalObject {
 public:
  explicit GlobalObject(int id)
      : text_(80, static_cast<char>('a' + id)), id_(id) {
    RuntimeGlobalEvents = RuntimeGlobalEvents * 10 + id_;
  }

  ~GlobalObject() {
    if (text_.size() != 80 || text_[0] != 'a' + id_) {
      RuntimeGlobalEvents = -1;
    } else {
      RuntimeGlobalEvents = RuntimeGlobalEvents * 10 + id_ + 2;
    }
  }

  bool valid() const { return text_.size() == 80; }

 private:
  std::string text_;
  int id_;
};

__attribute__((visibility("default"))) GlobalObject RuntimeFirst(1);
__attribute__((visibility("default"))) GlobalObject RuntimeSecond(2);

extern "C" int RuntimeCheckInitializers() {
  if (RuntimeGlobalEvents != 12 || !RuntimeFirst.valid() ||
      !RuntimeSecond.valid()) {
    return -120;
  }
  RuntimeGlobalCells = SymbianRuntimeAllocationCells();
  return 0;
}

extern "C" int RuntimeCheckFinalizers() {
  return RuntimeGlobalEvents == 1243 &&
                 SymbianRuntimeAllocationCells() == RuntimeGlobalCells - 2
             ? 0
             : -121;
}
