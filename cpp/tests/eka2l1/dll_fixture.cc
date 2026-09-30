// Independent research fixture producer using Nokia's EPL header/checksums.
#include <bit>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>

#include "validator_host_types.h"

#define INCLUDE_E32IMAGEHEADER_IMPLEMENTATION
#define __CPU_ARM
#define __EABI__
#include <f32image.h>

static_assert(std::endian::native == std::endian::little);
static_assert(sizeof(E32ImageHeaderV) == 156);

int main(int argc, char** argv) {
  if (argc != 3) {
    return 1;
  }
  std::ifstream input(argv[1], std::ios::binary);
  std::string bytes{std::istreambuf_iterator<char>(input),
                    std::istreambuf_iterator<char>()};
  E32ImageHeaderV header{};
  if (bytes.size() < sizeof(header) + 132 || bytes.size() > 65536) {
    return 2;
  }
  std::memcpy(&header, bytes.data(), sizeof(header));
  if (header.iCodeOffset != sizeof(header) || header.iCodeBase != 0x8000 ||
      header.iCompressionType != 0 || header.iDllRefTableCount != 0 ||
      header.ValidateWholeImage(
          bytes.data() + sizeof(header),
          static_cast<TUint>(bytes.size() - sizeof(header))) != 0) {
    return 3;
  }
  header.iUid1 = 0x10000079;
  header.iUid2 = 0x1000008d;
  header.iUid3 = 0xe0000810;
  header.iS.secure_id = header.iUid3;
  header.iFlags |= KImageDll;
  header.iExportDirOffset = static_cast<TUint>(bytes.size());
  header.iExportDirCount = 7;
  header.iCodeSize += 28;
  header.iUncompressedSize += 28;
  header.iExportDescSize = 1;
  header.iExportDescType = 1;
  header.iExportDesc[0] = 0x40;
  for (size_t i = 0; i < 7; ++i) {
    const uint32_t address =
        i == 6 ? 0x8081 : header.iCodeBase + header.iEntryPoint;
    bytes.append(reinterpret_cast<const char*>(&address), sizeof(address));
  }
  TUidType uids{};
  std::memcpy(&uids, &header.iUid1, sizeof(uids));
  header.iUidChecksum = TCheckedUid(uids).check;
  header.iHeaderCrc = KImageCrcInitialiser;
  uint32_t crc = 0;
  Mem::Crc32(crc, &header, sizeof(header));
  header.iHeaderCrc = crc;
  std::memcpy(bytes.data(), &header, sizeof(header));
  if (header.ValidateWholeImage(
          bytes.data() + sizeof(header),
          static_cast<TUint>(bytes.size() - sizeof(header))) != 0) {
    return 4;
  }
  std::ofstream output(argv[2], std::ios::binary);
  output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  return output ? 0 : 5;
}
