#include "GreedyLayoutStrategy.h"

#include "../../content/providers/WordProvider.h"
#include "../../rendering/TextRenderer.h"
#include "WString.h"

#ifdef ARDUINO
#include <Arduino.h>
#endif
#ifndef ARDUINO
#include "platform_stubs.h"
#endif
#include <cstdint>

GreedyLayoutStrategy::GreedyLayoutStrategy() {}

GreedyLayoutStrategy::~GreedyLayoutStrategy() {}

LayoutStrategy::PageLayout GreedyLayoutStrategy::layoutText(WordProvider& provider, TextRenderer& renderer,
                                                            const LayoutConfig& config) {
  const int16_t maxWidth = config.pageWidth - config.marginLeft - config.marginRight;
  const int16_t x = config.marginLeft;
  int16_t y = config.marginTop;
  const int16_t maxY = config.pageHeight - config.marginBottom;
  const int16_t lineHeight = (config.lineHeight > 0) ? config.lineHeight : 1;

  // Measure space width using renderer
  renderer.setFontStyle(FontStyle::REGULAR);
  renderer.getTextBounds(" ", 0, 0, nullptr, nullptr, &spaceWidth_, nullptr);

  PageLayout result;
  int startIndex = provider.getCurrentIndex();

  while (y < maxY) {
    // Hard stop: don't start a new line if it would cross into reserved bottom area
    if ((int32_t)y + (int32_t)lineHeight > (int32_t)maxY) {
      break;
    }
    bool isParagraphEnd = false;
    // getNextLine uses config.alignment as default, CSS overrides if present
    Line line = getNextLine(provider, renderer, maxWidth, isParagraphEnd, config.alignment);

    // Calculate positions for each word in the line
    if (!line.words.empty()) {
      // Calculate line width
      int16_t lineWidth = 0;
      for (size_t i = 0; i < line.words.size(); i++) {
        lineWidth += line.words[i].width;
        if (i < line.words.size() - 1) {
          lineWidth += spaceWidth_;
        }
      }

      int16_t xPos = x;
      if (line.alignment == ALIGN_CENTER) {
        xPos = x + (maxWidth - lineWidth) / 2;
      } else if (line.alignment == ALIGN_RIGHT) {
        xPos = x + maxWidth - lineWidth;
      }

      int16_t currentX = xPos;
      for (size_t i = 0; i < line.words.size(); i++) {
        line.words[i].x = currentX;
        line.words[i].y = y;
        currentX += line.words[i].width;
        if (i < line.words.size() - 1) {
          currentX += spaceWidth_;
        }
      }
    }

    result.lines.push_back(line);
    y += lineHeight;
    if (isParagraphEnd && !line.words.empty()) {
      const int16_t ps = (config.paragraphSpacing > 0) ? config.paragraphSpacing : 0;
      // Only apply paragraph spacing if it still fits above reserved bottom area
      if ((int32_t)y + (int32_t)ps <= (int32_t)maxY) {
        y += ps;
      } else {
        break;
      }
    }
  }

  result.endPosition = provider.getCurrentIndex();
  // reset the provider to the start index
  provider.setPosition(startIndex);

  return result;
}

void GreedyLayoutStrategy::renderPage(const PageLayout& layout, TextRenderer& renderer, const LayoutConfig& config) {
  const int16_t maxY = config.pageHeight - config.marginBottom;
  const int16_t lineHeight = (config.lineHeight > 0) ? config.lineHeight : 1;
  for (const auto& line : layout.lines) {
    // Skip/stop any lines that would draw into the reserved footer band.
    if (line.words.empty()) {
      continue;
    }
    if ((int32_t)line.words.front().y + (int32_t)lineHeight > (int32_t)maxY) {
      break;
    }
    for (const auto& word : line.words) {
      renderer.setFontStyle(word.style);
      renderer.setCursor(word.x, word.y);
      renderer.print(word.text);
    }
  }
}

LayoutStrategy::Line GreedyLayoutStrategy::test_getNextLine(WordProvider& provider, TextRenderer& renderer,
                                                            int16_t maxWidth, bool& isParagraphEnd) {
  return getNextLine(provider, renderer, maxWidth, isParagraphEnd, ALIGN_LEFT);
}
