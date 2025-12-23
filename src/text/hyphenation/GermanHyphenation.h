#ifndef GERMAN_HYPHENATION_H
#define GERMAN_HYPHENATION_H

#include "HyphenationStrategy.h"

class GermanHyphenation : public HyphenationStrategy {
 public:
  std::vector<size_t> hyphenate(const std::string& word, size_t minWordLength = 6,
                                size_t minFragmentLength = 3) override;

  Language getLanguage() const override {
    return Language::GERMAN;
  }
};

#endif  // GERMAN_HYPHENATION_H
