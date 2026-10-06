#include <png.h>

int main() {
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
  return 0;
}
