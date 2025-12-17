#pragma once

#include <cstdint>

// Enum for font styles (expandable for future styles)
enum class FontStyle { REGULAR = 0, BOLD, ITALIC, BOLD_ITALIC, HIDDEN };

// Minimal font struct used by our TextRenderer
typedef struct {
  uint16_t bitmapOffset;  ///< Pointer into font->bitmap
  uint32_t codepoint;     ///< Unicode codepoint for this glyph
  uint8_t width;          ///< Bitmap dimensions in pixels
  uint8_t height;
  uint8_t xAdvance;  ///< Distance to advance cursor (x axis)
  int8_t xOffset;    ///< X dist from cursor pos to UL corner
  int8_t yOffset;    ///< Y dist from cursor pos to UL corner
} SimpleGFXglyph;

typedef struct {
  const uint8_t* bitmap;           ///< Glyph bitmaps, concatenated
  const uint8_t* bitmap_gray_lsb;  ///< Glyph bitmaps, concatenated
  const uint8_t* bitmap_gray_msb;  ///< Glyph bitmaps, concatenated
  const SimpleGFXglyph* glyph;     ///< Glyph array (sorted by codepoint for binary search)
  uint16_t glyphCount;             ///< Number of entries in `glyph`.
  uint8_t yAdvance;                ///< Newline distance (y axis)
  // Optional metadata for better font management
  const char* name;  ///< Font name (e.g., "NotoSans")
  uint8_t size;      ///< Font size in points (for reference)
  FontStyle style;   ///< Style of this font variant
} SimpleGFXfont;

// New: Font family struct to group style variants
typedef struct {
  const char* familyName;           ///< Name of the font family (e.g., "NotoSans")
  const SimpleGFXfont* regular;     ///< Regular style (required)
  const SimpleGFXfont* bold;        ///< Bold variant (optional, nullptr if not loaded)
  const SimpleGFXfont* italic;      ///< Italic variant (optional)
  const SimpleGFXfont* boldItalic;  ///< Bold-italic variant (optional)
} FontFamily;

// Helper to find a glyph index by codepoint using binary search
// Returns -1 if the glyph is not found
int findGlyphIndex(const SimpleGFXfont* font, uint32_t codepoint);

// Helper to get a font variant from a family (returns nullptr if not available)
const SimpleGFXfont* getFontVariant(const FontFamily* family, FontStyle style);
