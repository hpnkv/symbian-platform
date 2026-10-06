#include <ft2build.h>
#include FT_FREETYPE_H

#include "absl/base/nullability.h"
#include "ui.h"

namespace {
constexpr char kFont[] =
    "STARTFONT 2.1\n"
    "FONT -misc-fixed-medium-r-normal--8-80-75-75-C-80-iso10646-1\n"
    "SIZE 8 75 75\n"
    "FONTBOUNDINGBOX 8 8 0 0\n"
    "STARTPROPERTIES 2\n"
    "FONT_ASCENT 8\n"
    "FONT_DESCENT 0\n"
    "ENDPROPERTIES\n"
    "CHARS 1\n"
    "STARTCHAR A\n"
    "ENCODING 65\n"
    "SWIDTH 500 0\n"
    "DWIDTH 8 0\n"
    "BBX 8 8 0 0\n"
    "BITMAP\n"
    "18\n24\n42\n7E\n42\n42\n42\n00\n"
    "ENDCHAR\n"
    "ENDFONT\n";
}  // namespace

int RunFeature(TUint32* absl_nonnull preview) {
  FT_Library library = nullptr;
  if (FT_Init_FreeType(&library) != 0) {
    return 1;
  }
  FT_Face face = nullptr;
  const FT_Byte* absl_nonnull bytes = reinterpret_cast<const FT_Byte*>(kFont);
  if (FT_New_Memory_Face(library, bytes, sizeof(kFont) - 1, 0, &face) != 0) {
    FT_Done_FreeType(library);
    return 2;
  }
  if (FT_Set_Pixel_Sizes(face, 0, 8) != 0 ||
      FT_Load_Char(face, 'A', FT_LOAD_RENDER) != 0 ||
      face->glyph->bitmap.width == 0 || face->glyph->bitmap.rows == 0 ||
      face->glyph->bitmap.buffer == nullptr) {
    FT_Done_Face(face);
    FT_Done_FreeType(library);
    return 3;
  }
  const FT_Bitmap& bitmap = face->glyph->bitmap;
  if (bitmap.width > 8 || bitmap.rows > 8 || bitmap.pitch <= 0) {
    FT_Done_Face(face);
    FT_Done_FreeType(library);
    return 4;
  }
  for (unsigned int y = 0; y < bitmap.rows; ++y) {
    for (unsigned int x = 0; x < bitmap.width; ++x) {
      if (bitmap.pixel_mode == FT_PIXEL_MODE_MONO) {
        preview[y * 8 + x] =
            bitmap.buffer[y * bitmap.pitch + x / 8] & (0x80 >> (x % 8))
                ? 0x00ffffff
                : 0;
      } else if (bitmap.pixel_mode == FT_PIXEL_MODE_GRAY) {
        preview[y * 8 + x] =
            bitmap.buffer[y * bitmap.pitch + x] != 0 ? 0x00ffffff : 0;
      }
    }
  }
  FT_Done_Face(face);
  FT_Done_FreeType(library);
  return 0;
}

int InvokeFeature(void* absl_nullable context) {
  if (context == nullptr) {
    return KErrArgument;
  }
  return RunFeature(static_cast<TUint32*>(context));
}

int main() {
  TUint32 preview[64] = {};
  return classic_demo_ui::Show(
      _L("FREETYPE FONT"), _L("Loads an embedded BDF font."),
      _L("Rasterizes the letter A."), _L("LETTER A RASTERIZED"), &InvokeFeature,
      preview, preview, 8, 8);
}
