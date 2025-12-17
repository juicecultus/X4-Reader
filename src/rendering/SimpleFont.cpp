#include "SimpleFont.h"

// Helper to find a glyph index by codepoint using binary search
// The glyph array must be sorted by codepoint
int findGlyphIndex(const SimpleGFXfont* font, uint32_t codepoint) {
  if (!font || !font->glyph || font->glyphCount == 0) {
    return -1;
  }

  int low = 0;
  int high = font->glyphCount - 1;

  while (low <= high) {
    int mid = low + (high - low) / 2;
    uint32_t midCodepoint = font->glyph[mid].codepoint;

    if (midCodepoint == codepoint) {
      return mid;
    } else if (midCodepoint < codepoint) {
      low = mid + 1;
    } else {
      high = mid - 1;
    }
  }

  return -1;  // Not found
}

// Helper to get a font variant from a family (returns nullptr if not available)
const SimpleGFXfont* getFontVariant(const FontFamily* family, FontStyle style) {
  if (!family) {
    return nullptr;
  }

  switch (style) {
    case FontStyle::REGULAR:
      return family->regular;
    case FontStyle::BOLD:
      return family->bold ? family->bold : family->regular;  // Fallback to regular
    case FontStyle::ITALIC:
      return family->italic ? family->italic : family->regular;
    case FontStyle::BOLD_ITALIC:
      return family->boldItalic ? family->boldItalic : (family->bold ? family->bold : family->regular);
    case FontStyle::HIDDEN:
      return family->regular;  // Use regular font but don't draw
    default:
      return family->regular;
  }
}