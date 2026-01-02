#include "FontManager.h"

#include "FontDefinitions.h"
#include "other/MenuFontBig.h"
#include "other/MenuFontSmall.h"
#include "other/MenuHeader.h"

// Font family (default to Bookerly26)
static FontFamily* currentFamily = &bookerly26Family;

FontFamily* getCurrentFontFamily() {
  return currentFamily;
}

void setCurrentFontFamily(FontFamily* family) {
  if (family)
    currentFamily = family;
}

// Simple fonts
static const SimpleGFXfont* mainFont = &MenuFontSmall;
static const SimpleGFXfont* titleFont = &MenuHeader;

const SimpleGFXfont* getMainFont() {
  return mainFont;
}

void setMainFont(const SimpleGFXfont* font) {
  if (font)
    mainFont = font;
}

const SimpleGFXfont* getTitleFont() {
  return titleFont;
}

void setTitleFont(const SimpleGFXfont* font) {
  if (font)
    titleFont = font;
}
