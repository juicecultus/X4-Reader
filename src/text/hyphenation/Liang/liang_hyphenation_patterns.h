#ifndef HYPHENATION_PATTERNS_H
#define HYPHENATION_PATTERNS_H

#include <cstddef>
#include <cstdint>

struct PatternC {
  const std::uint8_t* letters;
  const std::uint8_t* values;
  std::uint8_t letters_len;
  std::uint8_t values_len;
};

struct HyphenationPatterns {
  const PatternC* patterns;
  size_t count;
};

#endif  // HYPHENATION_PATTERNS_H
