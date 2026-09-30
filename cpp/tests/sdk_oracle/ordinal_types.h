#ifndef SYMBIAN_TESTS_SDK_ORACLE_ORDINAL_TYPES_H_
#define SYMBIAN_TESTS_SDK_ORACLE_ORDINAL_TYPES_H_

#include <cstdint>

// Fixed-width host declarations required by the extracted historical method.
// The original method text is generated unchanged from the pinned checkout.
using PLUINT32 = uint32_t;
using Elf32_Word = uint32_t;

struct Elf32_Ehdr {};

struct Elf32_Sym {
  uint32_t st_name;
  uint32_t st_value;
  uint32_t st_size;
  uint8_t st_info;
  uint8_t st_other;
  uint16_t st_shndx;
};

struct Elf32_Phdr {
  uint32_t p_type;
  uint32_t p_offset;
  uint32_t p_vaddr;
  uint32_t p_paddr;
  uint32_t p_filesz;
  uint32_t p_memsz;
  uint32_t p_flags;
  uint32_t p_align;
};

static_assert(sizeof(Elf32_Sym) == 16);
static_assert(sizeof(Elf32_Phdr) == 32);
constexpr int ESegmentRO = 1;
#define ELF_ENTRY_PTR(type, base, offset) \
  reinterpret_cast<type*>(reinterpret_cast<unsigned char*>(base) + (offset))

class ElfExecutable {
 public:
  Elf32_Ehdr* iElfHeader = nullptr;
  Elf32_Phdr* iCodeSegmentHdr = nullptr;
  PLUINT32 GetSymbolOrdinal(Elf32_Sym* symbol);
};

#endif  // SYMBIAN_TESTS_SDK_ORACLE_ORDINAL_TYPES_H_
