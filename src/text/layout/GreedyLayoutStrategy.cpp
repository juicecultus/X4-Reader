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
#include <cmath>

GreedyLayoutStrategy::GreedyLayoutStrategy() {}

GreedyLayoutStrategy::~GreedyLayoutStrategy() {}

LayoutStrategy::PageLayout GreedyLayoutStrategy::layoutText(WordProvider& provider, TextRenderer& renderer,
                                                            const LayoutConfig& config) {
  const int16_t maxWidth = config.pageWidth - config.marginLeft - config.marginRight;
  const int16_t x = config.marginLeft;

  // Calculate vertical centering offset based on maximum line capacity
  const int16_t availableHeight = config.pageHeight - config.marginTop - config.marginBottom;
  const int16_t maxLines = availableHeight / config.lineHeight;
  const int16_t totalContentHeight = maxLines * config.lineHeight - config.lineSpacing;
  const int16_t verticalPadding = (availableHeight - totalContentHeight) / 2;

  // Font height = lineHeight - lineSpacing
  // Y coordinate is the baseline, so add font height to ensure top of first line is visible
  int16_t y = config.marginTop + verticalPadding + (config.lineHeight - config.lineSpacing);
  const int16_t maxY = config.pageHeight - config.marginBottom;

  // Measure space width using renderer
  renderer.setFontStyle(FontStyle::REGULAR);
  renderer.getTextBounds(" ", 0, 0, nullptr, nullptr, &spaceWidth_, nullptr);

  PageLayout result;
  int startIndex = provider.getCurrentIndex();

  while (y < maxY) {
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
    y += config.lineHeight;
  }

  result.endPosition = provider.getCurrentIndex();
  // reset the provider to the start index
  provider.setPosition(startIndex);

  return result;
}

void GreedyLayoutStrategy::renderPage(const PageLayout& layout, TextRenderer& renderer, const LayoutConfig& config) {
  for (const auto& line : layout.lines) {
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
