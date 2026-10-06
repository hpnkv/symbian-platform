#include <cstddef>
#include <cstdio>
#include <cstdlib>

#include <jpeglib.h>

#include "absl/base/nullability.h"
#include "ui.h"

int RunFeature(TUint32* absl_nonnull preview) {
  jpeg_compress_struct encoder = {};
  jpeg_error_mgr encode_error = {};
  encoder.err = jpeg_std_error(&encode_error);
  jpeg_create_compress(&encoder);
  unsigned char* absl_nullable encoded = nullptr;
  unsigned long encoded_size = 0;
  jpeg_mem_dest(&encoder, &encoded, &encoded_size);
  encoder.image_width = 1;
  encoder.image_height = 1;
  encoder.input_components = 3;
  encoder.in_color_space = JCS_RGB;
  jpeg_set_defaults(&encoder);
  jpeg_set_quality(&encoder, 100, TRUE);
  jpeg_start_compress(&encoder, TRUE);
  unsigned char pixel[3] = {216, 32, 40};
  JSAMPROW row = pixel;
  jpeg_write_scanlines(&encoder, &row, 1);
  jpeg_finish_compress(&encoder);
  jpeg_destroy_compress(&encoder);
  if (encoded == nullptr || encoded_size == 0) {
    free(encoded);
    return 1;
  }

  jpeg_decompress_struct decoder = {};
  jpeg_error_mgr decode_error = {};
  decoder.err = jpeg_std_error(&decode_error);
  jpeg_create_decompress(&decoder);
  jpeg_mem_src(&decoder, encoded, encoded_size);
  if (jpeg_read_header(&decoder, TRUE) != JPEG_HEADER_OK) {
    jpeg_destroy_decompress(&decoder);
    free(encoded);
    return 2;
  }
  jpeg_start_decompress(&decoder);
  if (decoder.output_width != 1 || decoder.output_height != 1 ||
      decoder.output_components != 3) {
    jpeg_destroy_decompress(&decoder);
    free(encoded);
    return 3;
  }
  unsigned char decoded[3] = {};
  JSAMPROW output = decoded;
  jpeg_read_scanlines(&decoder, &output, 1);
  jpeg_finish_decompress(&decoder);
  jpeg_destroy_decompress(&decoder);
  free(encoded);
  if (decoded[0] < 180 || decoded[1] > 80 || decoded[2] > 90) {
    return 4;
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
      _L("PORTABLE JPEG"), _L("Encodes one red pixel."),
      _L("Decodes it and checks RGB."), _L("RED PIXEL DECODED"), &InvokeFeature,
      &preview, &preview, 1, 1);
}
