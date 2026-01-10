#ifndef TEXT_RENDERER_H
#define TEXT_RENDERER_H

#ifdef TEST_BUILD
#include "Arduino.h"
#else
#include "WString.h"
#endif

#include <cstddef>
#include <cstdint>

#include "SimpleFont.h"

class EInkDisplay;  // Forward declaration

class TextRenderer {
 public:
  enum Orientation {
    Portrait,                  // 480x800 logical coordinates
    LandscapeClockwise,        // 800x480 logical coordinates, rotated 180°
    PortraitInverted,          // 480x800 logical coordinates, inverted
    LandscapeCounterClockwise  // 800x480 logical coordinates, aligned with panel
  };

  // Bitmap selection for font rendering
  enum BitmapType {
    BITMAP_BW,        // Use the main black & white bitmap
    BITMAP_GRAY_LSB,  // Use the grayscale LSB bitmap
    BITMAP_GRAY_MSB   // Use the grayscale MSB bitmap
  };

  // Constructor
  TextRenderer(EInkDisplay& display);

  // Low-level pixel draw used by font blitting
  void drawPixel(int16_t x, int16_t y, bool state);

  void setOrientation(Orientation o) {
    orientation = o;
  }
  Orientation getOrientation() const {
    return orientation;
  }

  // Set which framebuffer to write to
  void setFrameBuffer(uint8_t* buffer);

  // Select which bitmap data to use from the font
  void setBitmapType(BitmapType type);

  // Minimal API used by the rest of the project
  void setFont(const SimpleGFXfont* f = nullptr);
  void setFontFamily(FontFamily* family);
  void setFontStyle(FontStyle style);
  void setTextColor(uint16_t c);
  void setCursor(int16_t x, int16_t y);
  size_t print(const char* s);
  size_t print(const String& s);

  // Measure text bounds for layout
  void getTextBounds(const char* str, int16_t x, int16_t y, int16_t* x1, int16_t* y1, uint16_t* w, uint16_t* h);

  // Color constants (0 = black, 1 = white for 1-bit display)
  static const uint16_t COLOR_BLACK = 0;
  static const uint16_t COLOR_WHITE = 1;

 private:
  EInkDisplay& display;
  const SimpleGFXfont* currentFont = nullptr;
  FontFamily* currentFamily = nullptr;
  FontStyle currentStyle = FontStyle::REGULAR;
  uint8_t* frameBuffer = nullptr;
  BitmapType bitmapType = BITMAP_BW;
  Orientation orientation = Portrait;
  int16_t cursorX = 0;
  int16_t cursorY = 0;
  uint16_t textColor = COLOR_BLACK;

  // Draw a single Unicode codepoint. Accepts a full Unicode codepoint
  // (decoded from UTF-8) so the renderer can support multi-byte UTF-8 input.
  void drawChar(uint32_t codepoint);
};

#endif
