#include "TrueTypeRenderer.h"

#include "../core/EInkDisplay.h"

#ifdef USE_M5UNIFIED
#include <bb_truetype.h>

// Static instance pointer
TrueTypeRenderer* TrueTypeRenderer::activeInstance = nullptr;

// bb_truetype instance (static to avoid repeated construction)
static bb_truetype g_ttf;

TrueTypeRenderer::TrueTypeRenderer(EInkDisplay& display) : display(display) {
  Serial.printf("[%lu] TrueTypeRenderer: Constructor, free heap: %d\n", millis(), ESP.getFreeHeap());
}

TrueTypeRenderer::~TrueTypeRenderer() {
  closeFont();
}

// Not used - keeping for compatibility
void TrueTypeRenderer::drawLineCallback(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint32_t color) {
}

bool TrueTypeRenderer::loadFont(const char* path) {
  Serial.printf("[%lu] TrueTypeRenderer: Loading font '%s'\n", millis(), path);
  printMemoryStats();
  
  closeFont();  // Close any previously open font
  
  fontFile = SD.open(path, FILE_READ);
  if (!fontFile) {
    Serial.printf("[%lu] TrueTypeRenderer: Failed to open font file\n", millis());
    return false;
  }
  
  Serial.printf("[%lu] TrueTypeRenderer: Font file opened, size: %d bytes\n", millis(), fontFile.size());
  
  // Set up the TTF renderer
  // Note: setTtfFile returns 1 on success, 0 on failure
  uint8_t result = g_ttf.setTtfFile(fontFile, 0);  // 0 = don't verify checksum (faster)
  if (result == 0) {
    Serial.printf("[%lu] TrueTypeRenderer: setTtfFile failed\n", millis());
    fontFile.close();
    return false;
  }
  
  activeInstance = this;
  
  // Use bb_truetype's native 1-bit framebuffer mode
  // Paper S3 display is 540x960 in portrait mode
  g_ttf.setFramebuffer(EInkDisplay::DISPLAY_WIDTH, EInkDisplay::DISPLAY_HEIGHT, 1, display.getFrameBuffer());
  
  // Set text boundary to full display
  g_ttf.setTextBoundary(0, EInkDisplay::DISPLAY_WIDTH, EInkDisplay::DISPLAY_HEIGHT);
  
  // Set default character size
  g_ttf.setCharacterSize(charSize);
  
  // Set text color: 0 = black (clear bit), 1 = white (set bit)
  // For 1-bit mode, use 0 for black text
  g_ttf.setTextColor(0, 0);
  
  fontLoaded = true;
  Serial.printf("[%lu] TrueTypeRenderer: Font loaded successfully\n", millis());
  printMemoryStats();
  
  return true;
}

void TrueTypeRenderer::closeFont() {
  if (fontLoaded) {
    g_ttf.end();
    fontFile.close();
    fontLoaded = false;
    if (activeInstance == this) {
      activeInstance = nullptr;
    }
    Serial.printf("[%lu] TrueTypeRenderer: Font closed\n", millis());
  }
}

void TrueTypeRenderer::setCharacterSize(uint16_t size) {
  charSize = size;
  if (fontLoaded) {
    g_ttf.setCharacterSize(size);
  }
}

void TrueTypeRenderer::setTextColor(uint8_t color) {
  textColor = color;
  if (fontLoaded) {
    // For 1-bit mode: 0 = black (clear bit), 1 = white (set bit)
    g_ttf.setTextColor(color, color);
  }
}

void TrueTypeRenderer::drawText(int16_t x, int16_t y, const char* text) {
  if (!fontLoaded) {
    Serial.printf("[%lu] TrueTypeRenderer: drawText called but no font loaded\n", millis());
    return;
  }
  
  g_ttf.textDraw(x, y, text);
}

uint16_t TrueTypeRenderer::getStringWidth(const char* text) {
  if (!fontLoaded) {
    return 0;
  }
  return g_ttf.getStringWidth(text);
}

void TrueTypeRenderer::printMemoryStats() {
  Serial.printf("[%lu] Memory - Free heap: %d, Min free: %d, PSRAM free: %d\n",
                millis(), ESP.getFreeHeap(), ESP.getMinFreeHeap(),
                ESP.getFreePsram());
}

#else
// Stub implementation for non-M5UNIFIED builds

TrueTypeRenderer* TrueTypeRenderer::activeInstance = nullptr;

TrueTypeRenderer::TrueTypeRenderer(EInkDisplay& display) : display(display) {}
TrueTypeRenderer::~TrueTypeRenderer() {}
bool TrueTypeRenderer::loadFont(const char* path) { return false; }
void TrueTypeRenderer::closeFont() {}
void TrueTypeRenderer::setCharacterSize(uint16_t size) { charSize = size; }
void TrueTypeRenderer::setTextColor(uint8_t color) { textColor = color; }
void TrueTypeRenderer::drawText(int16_t x, int16_t y, const char* text) {}
uint16_t TrueTypeRenderer::getStringWidth(const char* text) { return 0; }
void TrueTypeRenderer::printMemoryStats() {}
void TrueTypeRenderer::drawLineCallback(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint32_t color) {}

#endif
