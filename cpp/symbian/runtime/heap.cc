// Minimal guest ABI bridge. This file is compiled for Symbian ARM, not the host.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>

#include "abi.h"

namespace {

static_assert(sizeof(size_t) == 4);
static_assert(alignof(std::max_align_t) <= 8);

void* Allocate(size_t size) noexcept {
  if (size > static_cast<size_t>(std::numeric_limits<int>::max())) {
    return nullptr;
  }
  return SymbianRuntimeAllocate(size == 0 ? 1 : size);
}

void* AllocateAligned(size_t size, size_t alignment) noexcept {
  if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
    return nullptr;
  }
  if (alignment <= alignof(std::max_align_t)) {
    return Allocate(size);
  }
  const size_t maximum = std::numeric_limits<int>::max();
  if (alignment > maximum - sizeof(void*) ||
      size > maximum - alignment - sizeof(void*)) {
    return nullptr;
  }
  void* original = Allocate(size + alignment + sizeof(void*));
  if (original == nullptr) {
    return nullptr;
  }
  const auto first = reinterpret_cast<uintptr_t>(original) + sizeof(void*);
  const auto aligned = (first + alignment - 1) & ~(alignment - 1);
  auto* result = reinterpret_cast<void*>(aligned);
  static_cast<void**>(result)[-1] = original;
  return result;
}

void FreeAligned(void* pointer, size_t alignment) noexcept {
  if (pointer == nullptr) {
    return;
  }
  SymbianRuntimeFree(alignment <= alignof(std::max_align_t)
                         ? pointer
                         : static_cast<void**>(pointer)[-1]);
}

[[noreturn]] void AllocationFailure() {
  // Returning nullptr from ordinary new violates its contract. This bounded
  // no-exceptions profile terminates the guest with KErrNoMemory instead.
  SymbianRuntimeExit(SymbianRuntimeExitReason::kOutOfMemory);
}

}  // namespace

void* operator new(size_t size) {
  void* result = Allocate(size);
  if (result == nullptr) {
    AllocationFailure();
  }
  return result;
}

void* operator new[](size_t size) {
  return ::operator new(size);
}

void* operator new(size_t size, const std::nothrow_t&) noexcept {
  return Allocate(size);
}

void* operator new[](size_t size, const std::nothrow_t& tag) noexcept {
  return ::operator new(size, tag);
}

void operator delete(void* pointer) noexcept {
  SymbianRuntimeFree(pointer);
}

void operator delete[](void* pointer) noexcept {
  ::operator delete(pointer);
}

void operator delete(void* pointer, size_t) noexcept {
  ::operator delete(pointer);
}

void operator delete[](void* pointer, size_t) noexcept {
  ::operator delete(pointer);
}

void operator delete(void* pointer, const std::nothrow_t&) noexcept {
  ::operator delete(pointer);
}

void operator delete[](void* pointer, const std::nothrow_t&) noexcept {
  ::operator delete(pointer);
}

void* operator new(size_t size, std::align_val_t alignment) {
  void* result = AllocateAligned(size, static_cast<size_t>(alignment));
  if (result == nullptr) {
    AllocationFailure();
  }
  return result;
}

void* operator new[](size_t size, std::align_val_t alignment) {
  return ::operator new(size, alignment);
}

void* operator new(size_t size, std::align_val_t alignment,
                   const std::nothrow_t&) noexcept {
  return AllocateAligned(size, static_cast<size_t>(alignment));
}

void* operator new[](size_t size, std::align_val_t alignment,
                     const std::nothrow_t& tag) noexcept {
  return ::operator new(size, alignment, tag);
}

void operator delete(void* pointer, std::align_val_t alignment) noexcept {
  FreeAligned(pointer, static_cast<size_t>(alignment));
}

void operator delete[](void* pointer, std::align_val_t alignment) noexcept {
  ::operator delete(pointer, alignment);
}

void operator delete(void* pointer, size_t,
                     std::align_val_t alignment) noexcept {
  ::operator delete(pointer, alignment);
}

void operator delete[](void* pointer, size_t,
                       std::align_val_t alignment) noexcept {
  ::operator delete(pointer, alignment);
}

void operator delete(void* pointer, std::align_val_t alignment,
                     const std::nothrow_t&) noexcept {
  ::operator delete(pointer, alignment);
}

void operator delete[](void* pointer, std::align_val_t alignment,
                       const std::nothrow_t&) noexcept {
  ::operator delete(pointer, alignment);
}

namespace std {
inline namespace _LIBCPP_ABI_NAMESPACE {

[[noreturn]] void __libcpp_verbose_abort(const char*, ...) noexcept {
  SymbianRuntimeExit(SymbianRuntimeExitReason::kRuntimeContractFailure);
}

}  // namespace _LIBCPP_ABI_NAMESPACE
}  // namespace std

// The original compiler-rt sources supply memcpy/memmove and aligned aliases.
// Remaining compiler-generated memory operations reuse the SDK's real routines.

extern "C" void __aeabi_memset(void* to, size_t size, int value) {
  memset(to, value, size);
}

extern "C" void __aeabi_memclr(void* to, size_t size) {
  memset(to, 0, size);
}

extern "C" void __aeabi_memclr4(void* to, size_t size) {
  __aeabi_memclr(to, size);
}

extern "C" void __aeabi_memclr8(void* to, size_t size) {
  __aeabi_memclr(to, size);
}

// No OpenC DLL is loaded by this profile. It provides only the C operation
// reached by the tested string paths, without borrowing that DLL's TLS state.
extern "C" size_t strlen(const char* text) {
  const char* end = text;
  while (*end != '\0') {
    ++end;
  }
  return static_cast<size_t>(end - text);
}

extern "C" int memcmp(const void* left, const void* right, size_t size) {
  const auto* a = static_cast<const unsigned char*>(left);
  const auto* b = static_cast<const unsigned char*>(right);
  for (size_t i = 0; i < size; ++i) {
    if (a[i] != b[i]) {
      return static_cast<int>(a[i]) - static_cast<int>(b[i]);
    }
  }
  return 0;
}

// The EABI divide-by-zero hook is platform policy, not an LLVM source edit.
extern "C" int __aeabi_idiv0(int) {
  SymbianRuntimeExit(SymbianRuntimeExitReason::kRuntimeContractFailure);
}
