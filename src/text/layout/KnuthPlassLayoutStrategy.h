#ifndef KNUTH_PLASS_LAYOUT_STRATEGY_H
#define KNUTH_PLASS_LAYOUT_STRATEGY_H

#include <vector>

#include "LayoutStrategy.h"

class KnuthPlassLayoutStrategy : public LayoutStrategy {
 public:
  KnuthPlassLayoutStrategy();
  ~KnuthPlassLayoutStrategy();

  // Test support: check if line count mismatch occurred
  bool hasLineCountMismatch() const {
    return lineCountMismatch_;
  }
  int getExpectedLineCount() const {
    return expectedLineCount_;
  }
  int getActualLineCount() const {
    return actualLineCount_;
  }
  void resetLineCountMismatch() {
    lineCountMismatch_ = false;
    expectedLineCount_ = 0;
    actualLineCount_ = 0;
  }

  Type getType() const override {
    return KNUTH_PLASS;
  }

  // Main interface implementation
  PageLayout layoutText(WordProvider& provider, TextRenderer& renderer, const LayoutConfig& config) override;
  void renderPage(const PageLayout& layout, TextRenderer& renderer, const LayoutConfig& config) override;

 private:
  // spaceWidth_ is defined in base class

  struct ParagraphLayoutInfo {
    std::vector<Word> words;
    std::vector<size_t> breaks;
    bool paragraphEnd;
    int16_t yStart;
  };

  // Knuth-Plass parameters
  static constexpr int32_t INFINITY_PENALTY = 1000000;
  static constexpr int32_t HYPHEN_PENALTY = 50;
  static constexpr int32_t FITNESS_DEMERITS = 100;

  // Node for dynamic programming
  struct Node {
    size_t position;         // Word index
    size_t line;             // Line number
    int32_t totalDemerits;   // Total demerits up to this point
    int16_t totalWidth;      // Width accumulated up to this position
    int prevBreak;           // Previous break point index (-1 if none)
  };

  // Helper methods
  std::vector<size_t> calculateBreaks(const std::vector<Word>& words, int16_t maxWidth);
  int32_t calculateBadness(int16_t actualWidth, int16_t targetWidth);
  int32_t calculateDemerits(int32_t badness, bool isLastLine);

  // Line count mismatch tracking for testing
  bool lineCountMismatch_ = false;
  int expectedLineCount_ = 0;
  int actualLineCount_ = 0;
};

#endif
