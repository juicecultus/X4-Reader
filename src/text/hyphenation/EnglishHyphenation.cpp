#include "EnglishHyphenation.h"

#include "Liang/hyph-en-us.h"
#include "Liang/hyphenation.h"

std::vector<size_t> EnglishHyphenation::hyphenate(const std::string& word, size_t minWordLength,
                                                  size_t minFragmentLength) {
  const int MAX_POSITIONS = 32;
  int out_positions[MAX_POSITIONS];

  int count = liang_hyphenate(word.c_str(), 2, 2, '.', out_positions, MAX_POSITIONS, en_us_patterns);

  std::vector<size_t> positions;
  for (int i = 0; i < count; ++i) {
    positions.push_back(static_cast<size_t>(out_positions[i]));
  }

  return positions;
}