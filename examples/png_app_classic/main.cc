#include <png.h>

#include "ui.h"

int RunFeature(TUint32* absl_nonnull preview) {
  const png_byte pixel[4] = {19, 83, 147, 211};
  png_image writer = {};
  writer.version = PNG_IMAGE_VERSION;
  writer.width = 1;
  writer.height = 1;
  writer.format = PNG_FORMAT_RGBA;
  png_alloc_size_t encoded_size = 0;
  if (!png_image_write_to_memory(&writer, nullptr, &encoded_size, 0, pixel, 0,
                                 nullptr) ||
      encoded_size > 256) {
    png_image_free(&writer);
    return 1;
  }
  png_byte encoded[256] = {};
  if (!png_image_write_to_memory(&writer, encoded, &encoded_size, 0, pixel, 0,
                                 nullptr)) {
    png_image_free(&writer);
    return 2;
  }
  png_image_free(&writer);

  png_image reader = {};
  reader.version = PNG_IMAGE_VERSION;
  if (!png_image_begin_read_from_memory(&reader, encoded, encoded_size)) {
    png_image_free(&reader);
    return 3;
  }
  reader.format = PNG_FORMAT_RGBA;
  png_byte decoded[4] = {};
  if (reader.width != 1 || reader.height != 1 ||
      !png_image_finish_read(&reader, nullptr, decoded, 0, nullptr)) {
    png_image_free(&reader);
    return 4;
  }
  png_image_free(&reader);
  for (unsigned index = 0; index < 4; ++index) {
    if (decoded[index] != pixel[index]) {
      return 5;
    }
  }
  *preview = (static_cast<TUint32>(decoded[0]) << 16) |
             (static_cast<TUint32>(decoded[1]) << 8) | decoded[2];
  return 0;
}

int InvokeFeature(void* absl_nullable context) {
  if (context == nullptr) {
    return KErrArgument;
  }
  return RunFeature(static_cast<TUint32*>(context));
}

int main() {
  TUint32 preview = 0;
  return classic_demo_ui::Show(
      _L("PORTABLE PNG"), _L("Encodes one RGBA pixel."),
      _L("Decodes and checks it."), _L("RGBA PIXEL DECODED"), &InvokeFeature,
      &preview, &preview, 1, 1);
}
