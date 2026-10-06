#include <cstdint>

#include <absl/base/nullability.h>

namespace {

// ARM EHABI control-block layout, shared with the original drtaeabi runtime.
// Reference: kernel/eka/compsupp/symaehabi/{unwinder.h,cppsemantics.cpp}.
struct alignas(8) ControlBlock {
  unsigned char exception_class[8];
  std::uint32_t exception_cleanup;
  std::uint32_t unwinder_cache[5];
  std::uint32_t barrier_sp;
  std::uint32_t barrier[5];
  std::uint32_t cleanup_cache[4];

  struct Cache {
    std::uint32_t function;
    const std::uint32_t* absl_nonnull table;
    std::uint32_t additional;
    std::uint32_t reserved;
  } cache;
};

static_assert(sizeof(ControlBlock) == 88);
static_assert(sizeof(void*) == 4);

extern "C" int _Unwind_VRS_Get(void* absl_nonnull context, int register_class,
                               std::uint32_t number, int representation,
                               void* absl_nonnull value);
extern "C" int _Unwind_VRS_Set(void* absl_nonnull context, int register_class,
                               std::uint32_t number, int representation,
                               void* absl_nonnull value);
extern "C" int __aeabi_unwind_cpp_pr0(int state,
                                      ControlBlock* absl_nonnull block,
                                      void* absl_nonnull context);
extern "C" int __aeabi_unwind_cpp_pr1(int state,
                                      ControlBlock* absl_nonnull block,
                                      void* absl_nonnull context);
extern "C" int __cxa_type_match(ControlBlock* absl_nonnull block,
                                const void* absl_nonnull type, bool reference,
                                void* absl_nullable* absl_nonnull matched);
extern "C" bool __cxa_begin_cleanup(ControlBlock* absl_nonnull block);

constexpr int kFailure = 9;
constexpr int kFound = 6;
constexpr int kInstall = 7;

bool Register(void* absl_nonnull context, std::uint32_t number,
              std::uint32_t* absl_nonnull value, bool write = false) {
  return (write ? _Unwind_VRS_Set(context, 0, number, 0, value)
                : _Unwind_VRS_Get(context, 0, number, 0, value)) == 0;
}

// Clang's generic tables encode three initial bytes and an extension count.
// Adapt only the unwind instruction stream to the original compact decoder;
// its handler-table format differs and is never asked to parse GNU LSDA.
int Continue(int state, ControlBlock* absl_nonnull block,
             void* absl_nonnull context) {
  const auto saved = block->cache;
  const auto* absl_nonnull instructions = saved.table + 1;
  const std::uint32_t count = instructions[0] >> 24;
  std::uint32_t compact[257];
  int result;
  if (count == 0) {
    block->cache.table = instructions;
    block->cache.additional = 1;
    result = __aeabi_unwind_cpp_pr0(state, block, context);
  } else {
    if (count >= 255) {
      return kFailure;
    }
    compact[0] = ((count + 1) << 16) | ((instructions[0] >> 8) & 65535);
    for (std::uint32_t i = 0; i <= count; ++i) {
      compact[i + 1] = (instructions[i] << 24) |
                       (i < count ? instructions[i + 1] >> 8 : 0xb0b0b0);
    }
    block->cache.table = compact;
    block->cache.additional = 1;
    result = __aeabi_unwind_cpp_pr1(state, block, context);
  }
  block->cache = saved;
  return result;
}

struct Cursor {
  const unsigned char* absl_nonnull position;
  const unsigned char* absl_nonnull end;

  bool Byte(std::uint32_t* absl_nonnull value) {
    if (position >= end) {
      return false;
    }
    *value = *position++;
    return true;
  }

  bool Leb(std::uint32_t* absl_nonnull value, bool signed_value = false) {
    *value = 0;
    for (unsigned int shift = 0; shift < 35; shift += 7) {
      std::uint32_t byte;
      if (!Byte(&byte) || (shift == 28 && (byte & 0x70) != 0 &&
                           (!signed_value || (byte & 0x70) != 0x70))) {
        return false;
      }
      *value |= (byte & 127) << shift;
      if (!(byte & 128)) {
        if (signed_value && (byte & 64) && shift < 28) {
          *value |= ~std::uint32_t{0} << (shift + 7);
        }
        return true;
      }
    }
    return false;
  }
};

struct Handler {
  std::uint32_t pad = 0;
  std::uint32_t selector = 0;
  void* absl_nullable matched = nullptr;
};

bool Scan(ControlBlock* absl_nonnull block, std::uint32_t pc, bool search,
          Handler* absl_nonnull result) {
  // Symbian caches the current image's four-word exception descriptor here.
  const auto* absl_nonnull descriptor =
      reinterpret_cast<const std::uint32_t*>(block->unwinder_cache[4]);
  const auto end = descriptor[3];
  const auto table = reinterpret_cast<std::uintptr_t>(block->cache.table);
  if (table < (descriptor[2] & ~std::uint32_t{3}) || table > end - 8) {
    return false;
  }
  const auto count = block->cache.table[1] >> 24;
  const auto lsda = table + (2 + count) * 4;
  if (lsda >= end) {
    return false;
  }
  Cursor cursor{reinterpret_cast<const unsigned char*>(lsda),
                reinterpret_cast<const unsigned char*>(end)};
  std::uint32_t landing_encoding, type_encoding, type_offset = 0;
  if (!cursor.Byte(&landing_encoding) || landing_encoding != 255 ||
      !cursor.Byte(&type_encoding)) {
    return false;
  }
  const unsigned char* absl_nullable types = nullptr;
  if (type_encoding != 255) {
    if ((type_encoding != 0 && type_encoding != 0x10 &&
         type_encoding != 0x90) ||
        !cursor.Leb(&type_offset) ||
        type_offset >
            static_cast<std::uint32_t>(cursor.end - cursor.position)) {
      return false;
    }
    types = cursor.position + type_offset;
  }
  std::uint32_t call_encoding, length;
  if (!cursor.Byte(&call_encoding) || call_encoding != 1 ||
      !cursor.Leb(&length) ||
      length > static_cast<std::uint32_t>(cursor.end - cursor.position)) {
    return false;
  }
  const auto* absl_nonnull actions = cursor.position + length;
  cursor.end = actions;
  while (cursor.position < cursor.end) {
    std::uint32_t start, size, pad, action;
    if (!cursor.Leb(&start) || !cursor.Leb(&size) || !cursor.Leb(&pad) ||
        !cursor.Leb(&action)) {
      return false;
    }
    const auto offset = (pc & ~std::uint32_t{1}) - 1 - block->cache.function;
    if (offset < start || offset - start >= size) {
      continue;
    }
    if (pad == 0) {
      return true;
    }
    result->pad = block->cache.function + pad;
    if (action == 0) {
      if (search) {
        result->pad = 0;
      }
      return true;
    }
    const auto available = end - reinterpret_cast<std::uintptr_t>(actions);
    if (action > available) {
      return false;
    }
    Cursor entry{actions + action - 1,
                 reinterpret_cast<const unsigned char*>(end)};
    bool cleanup = false;
    for (unsigned int depth = 0; depth < 1024; ++depth) {
      std::uint32_t selector, next;
      if (!entry.Leb(&selector, true)) {
        return false;
      }
      const auto* absl_nonnull next_base = entry.position;
      if (!entry.Leb(&next, true) || static_cast<std::int32_t>(selector) < 0) {
        return false;  // Dynamic exception specifications are unsupported.
      }
      if (selector == 0) {
        cleanup = true;
      } else {
        if (types == nullptr || selector > type_offset / 4) {
          return false;
        }
        const auto* absl_nonnull pointer =
            reinterpret_cast<const std::uint32_t*>(types - selector * 4);
        const auto target = *pointer;
        const void* absl_nullable type =
            target == 0
                ? nullptr
                : *reinterpret_cast<const void* const*>(
                      reinterpret_cast<std::uintptr_t>(pointer) + target);
        void* absl_nullable matched = nullptr;
        if (type == nullptr || __cxa_type_match(block, type, false, &matched)) {
          result->selector = selector;
          result->matched = matched;
          return true;
        }
      }
      if (next == 0) {
        if (search || !cleanup) {
          result->pad = 0;
        }
        return true;
      }
      const auto address = reinterpret_cast<std::uintptr_t>(next_base) + next;
      if (address < reinterpret_cast<std::uintptr_t>(actions) ||
          address >= end) {
        return false;
      }
      entry.position = reinterpret_cast<const unsigned char*>(address);
    }
    return false;
  }
  return true;
}

}  // namespace

extern "C" int __gxx_personality_v0(int state, ControlBlock* absl_nonnull block,
                                    void* absl_nonnull context) {
  if (state < 0 || state > 2) {
    return kFailure;
  }
  if (state == 2) {
    return Continue(state, block, context);
  }
  std::uint32_t pc, sp;
  Handler handler;
  if (!Register(context, 15, &pc) || !Register(context, 13, &sp) ||
      !Scan(block, pc, state == 0, &handler)) {
    return kFailure;
  }
  if (handler.pad == 0) {
    return Continue(state, block, context);
  }
  if (state == 0) {
    block->barrier_sp = sp;
    block->barrier[0] = reinterpret_cast<std::uintptr_t>(handler.matched);
    block->barrier[3] = handler.pad;
    block->barrier[4] = handler.selector;
    return kFound;
  }
  if (block->barrier_sp == sp) {
    handler.pad = block->barrier[3];
    handler.selector = block->barrier[4];
  } else {
    if (!__cxa_begin_cleanup(block)) {
      return kFailure;
    }
    handler.selector = 0;
  }
  std::uint32_t exception = reinterpret_cast<std::uintptr_t>(block);
  handler.pad |= pc & 1;  // Preserve the interrupted frame's Thumb state.
  if (!Register(context, 0, &exception, true) ||
      !Register(context, 1, &handler.selector, true) ||
      !Register(context, 15, &handler.pad, true)) {
    return kFailure;
  }
  return kInstall;
}
