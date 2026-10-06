#include <absl/base/nullability.h>
#include <mimalloc.h>

extern "C" int SymbianRuntimeMimallocSdkProbe() {
  auto* absl_nullable bytes = static_cast<unsigned char*>(mi_malloc(64));
  if (bytes == nullptr || mi_usable_size(bytes) < 64) {
    mi_free(bytes);
    return -183;
  }
  bytes[0] = 0x32;
  bytes[63] = 0x5a;
  auto* absl_nullable grown =
      static_cast<unsigned char*>(mi_realloc(bytes, 128));
  if (grown == nullptr) {
    mi_free(bytes);
    return -184;
  }
  if (grown[0] != 0x32 || grown[63] != 0x5a || mi_usable_size(grown) < 128) {
    mi_free(grown);
    return -185;
  }
  mi_free(grown);
  mi_collect(false);
  return 0;
}
