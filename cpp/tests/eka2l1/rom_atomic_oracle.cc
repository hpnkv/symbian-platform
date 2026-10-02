// SPDX-License-Identifier: GPL-3.0-or-later
// Read-only diagnostic using EKA2L1's original compressed E32 parser.
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "common/buffer.h"
#include "config/config.h"
#include "loader/romimage.h"
#include "mem/mem.h"

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "usage: symbian_rom_atomic_oracle EUSER.dll [...]\n";
    return 2;
  }
  for (int argument = 1; argument < argc; ++argument) {
    std::ifstream input(argv[argument], std::ios::binary);
    if (!input) {
      std::cerr << argv[argument] << ": cannot open\n";
      return 1;
    }
    std::vector<char> bytes(std::istreambuf_iterator<char>{input}, {});
    eka2l1::common::ro_buf_stream stream(
        reinterpret_cast<std::uint8_t*>(bytes.data()), bytes.size());
    eka2l1::config::state config;
    eka2l1::memory_system memory(nullptr, &config,
                                 eka2l1::mem::mem_model_type::multiple, false);
    // The original parser's epoc91 path reads an unmapped ROM-image export
    // table from the dump. EKA2 and later share this 120-byte image header;
    // the normal epoc10 path expects the image already mapped in memory.
    const auto parsed =
        eka2l1::loader::parse_romimg(&stream, &memory, epocver::epoc91);
    if (!parsed) {
      std::cerr << argv[argument] << ": EKA2L1 ROM parser rejected image\n";
      return 1;
    }
    const auto& image = *parsed;
    std::cout << argv[argument] << " exports=" << image.exports.size()
              << " code_base=0x" << std::hex << image.header.code_address
              << std::dec << '\n';
    // Ordinals from the pinned EABI euseru.def. Their presence alone does not
    // prove that an older ROM assigns the same operation to that ordinal.
    for (const auto [name, ordinal] :
         {std::pair{"add64", 2281}, std::pair{"cas64", 2329},
          std::pair{"load64", 2357}, std::pair{"store64", 2361},
          std::pair{"swap64", 2373}}) {
      std::cout << "  " << name << " ordinal=" << ordinal;
      if (image.exports.size() < ordinal) {
        std::cout << " absent\n";
        continue;
      }
      const std::uint32_t address = image.exports[ordinal - 1];
      const std::uint32_t code_address = address & ~std::uint32_t{1};
      std::cout << " address=0x" << std::hex << address << std::dec;
      if (code_address < image.header.code_address ||
          code_address - image.header.code_address >= image.header.code_size) {
        std::cout << " outside-code\n";
        continue;
      }
      const auto offset =
          static_cast<std::size_t>(
              eka2l1::loader::rom_image_header_file_size(epocver::epoc91)) +
          code_address - image.header.code_address;
      if (offset >= bytes.size()) {
        std::cout << " truncated\n";
        continue;
      }
      std::cout << " bytes=";
      for (std::size_t index = offset;
           index < bytes.size() && index < offset + 64; ++index) {
        std::cout << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<unsigned>(
                         static_cast<unsigned char>(bytes[index]));
      }
      std::cout << std::dec << '\n';
    }
  }
  return 0;
}
