#include <zlib.h>

#include "ui.h"

int RunFeature(void* absl_nullable) {
  const Bytef plain[] = "portable zlib on Symbian";
  Bytef compressed[96] = {};
  uLongf compressed_size = sizeof(compressed);
  if (compress2(compressed, &compressed_size, plain, sizeof(plain),
                Z_BEST_COMPRESSION) != Z_OK) {
    return 1;
  }
  Bytef restored[sizeof(plain)] = {};
  uLongf restored_size = sizeof(restored);
  if (uncompress(restored, &restored_size, compressed, compressed_size) !=
      Z_OK) {
    return 2;
  }
  if (restored_size != sizeof(plain)) {
    return 3;
  }
  for (unsigned index = 0; index < sizeof(plain); ++index) {
    if (restored[index] != plain[index]) {
      return 4;
    }
  }
  return crc32(0, restored, restored_size) == crc32(0, plain, sizeof(plain))
             ? 0
             : 5;
}

int main() {
  return classic_demo_ui::Show(
      _L("PORTABLE ZLIB"), _L("Compresses a short message."),
      _L("Restores it and checks CRC."), _L("CRC + ROUND TRIP MATCH"),
      &RunFeature, nullptr);
}
