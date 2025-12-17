#include "FileWordProvider.h"

#include <Arduino.h>

#include "WString.h"

// ESC-based format constants:
// Format: ESC + command byte (2 bytes total, fixed length)
// Alignment commands (start of line): ESC + 'L'(left), 'R'(right), 'C'(center), 'J'(justify)
// Style commands (inline): ESC + 'B'(bold on), 'b'(bold off), 'I'(italic on), 'i'(italic off),
//                          'X'(bold+italic on), 'x'(bold+italic off), 'H'(hidden on), 'h'(hidden off)
static constexpr char ESC_CHAR = '\x1B';

// Helper functions for mapping ESC command chars to alignments / styles
// Alignment command helpers: we treat uppercase letters as 'start' tokens and lowercase as 'end'
static bool tryGetAlignmentStart(char cmd, TextAlign* out) {
  switch (cmd) {
    case 'L':
      if (out)
        *out = TextAlign::Left;
      return true;
    case 'R':
      if (out)
        *out = TextAlign::Right;
      return true;
    case 'C':
      if (out)
        *out = TextAlign::Center;
      return true;
    case 'J':
      if (out)
        *out = TextAlign::Justify;
      return true;
  }
  return false;
}

static bool tryGetAlignmentEnd(char cmd, TextAlign* out) {
  switch (cmd) {
    case 'l':
      if (out)
        *out = TextAlign::Left;
      return true;
    case 'r':
      if (out)
        *out = TextAlign::Right;
      return true;
    case 'c':
      if (out)
        *out = TextAlign::Center;
      return true;
    case 'j':
      if (out)
        *out = TextAlign::Justify;
      return true;
  }
  return false;
}

static bool tryGetStyleForward(char cmd, FontStyle* out) {
  switch (cmd) {
    case 'B':
      if (out)
        *out = FontStyle::BOLD;
      return true;
    case 'b':
      if (out)
        *out = FontStyle::REGULAR;
      return true;
    case 'I':
      if (out)
        *out = FontStyle::ITALIC;
      return true;
    case 'i':
      if (out)
        *out = FontStyle::REGULAR;
      return true;
    case 'X':
      if (out)
        *out = FontStyle::BOLD_ITALIC;
      return true;
    case 'x':
      if (out)
        *out = FontStyle::REGULAR;
      return true;
    case 'H':
      if (out)
        *out = FontStyle::HIDDEN;
      return true;
    case 'h':
      if (out)
        *out = FontStyle::REGULAR;
      return true;
  }
  return false;
}

static bool tryGetStyleBackward(char cmd, FontStyle* out) {
  switch (cmd) {
    case 'B':
      if (out)
        *out = FontStyle::REGULAR;
      return true;
    case 'b':
      if (out)
        *out = FontStyle::BOLD;
      return true;
    case 'I':
      if (out)
        *out = FontStyle::REGULAR;
      return true;
    case 'i':
      if (out)
        *out = FontStyle::ITALIC;
      return true;
    case 'X':
      if (out)
        *out = FontStyle::REGULAR;
      return true;
    case 'x':
      if (out)
        *out = FontStyle::BOLD_ITALIC;
      return true;
    case 'H':
      if (out)
        *out = FontStyle::REGULAR;
      return true;
    case 'h':
      if (out)
        *out = FontStyle::HIDDEN;
      return true;
  }
  return false;
}

static bool isEscCommandChar(char cmd) {
  return tryGetAlignmentStart(cmd, nullptr) || tryGetAlignmentEnd(cmd, nullptr) || tryGetStyleForward(cmd, nullptr);
}

FileWordProvider::FileWordProvider(const char* path, size_t bufSize) : bufSize_(bufSize) {
  file_ = SD.open(path);
  if (!file_) {
    fileSize_ = 0;
    buf_ = nullptr;
    return;
  }
  fileSize_ = file_.size();
  index_ = 0;
  prevIndex_ = 0;
  buf_ = (uint8_t*)malloc(bufSize_);
  bufStart_ = 0;
  bufLen_ = 0;
  // Skip UTF-8 BOM at start of file if present so it doesn't appear as a word
  skipUtf8BomIfPresent();
  // Compute paragraph alignment for initial position
  computeParagraphAlignmentForPosition(index_);
}

FileWordProvider::~FileWordProvider() {
  if (file_)
    file_.close();
  if (buf_)
    free(buf_);
}

bool FileWordProvider::hasNextWord() {
  return index_ < fileSize_;
}

bool FileWordProvider::hasPrevWord() {
  return index_ > 0;
}

char FileWordProvider::charAt(size_t pos) {
  if (pos >= fileSize_)
    return '\0';
  if (!ensureBufferForPos(pos))
    return '\0';
  return (char)buf_[pos - bufStart_];
}

bool FileWordProvider::ensureBufferForPos(size_t pos) {
  if (!file_ || !buf_)
    return false;
  if (pos >= bufStart_ && pos < bufStart_ + bufLen_)
    return true;

  // Center buffer around pos when possible
  size_t start = (pos > bufSize_ / 2) ? (pos - bufSize_ / 2) : 0;
  if (start + bufSize_ > fileSize_) {
    if (fileSize_ > bufSize_)
      start = fileSize_ - bufSize_;
    else
      start = 0;
  }

  if (!file_.seek(start))
    return false;
  size_t r = file_.read(buf_, bufSize_);
  if (r == 0)
    return false;
  bufStart_ = start;
  bufLen_ = r;
  return true;
}

// Check if position has an ESC token (ESC + command byte = 2 bytes)
// Returns 2 if valid ESC token, 0 otherwise
// If processStyle is false, only checks validity without modifying state.
size_t FileWordProvider::parseEscTokenAtPos(size_t pos, TextAlign* outAlignment, bool processStyle) {
  if (pos + 1 >= fileSize_)
    return 0;
  char c = charAt(pos);
  if (c != ESC_CHAR)
    return 0;

  char cmd = charAt(pos + 1);

  TextAlign align;
  if (tryGetAlignmentStart(cmd, &align)) {
    if (outAlignment)
      *outAlignment = align;
    if (processStyle)
      currentParagraphAlignment_ = align;
    return 2;
  }
  // Closing paragraph alignment token: treat as an ESC token but it resets alignment
  if (tryGetAlignmentEnd(cmd, &align)) {
    if (outAlignment)
      *outAlignment = TextAlign::None;
    if (processStyle)
      currentParagraphAlignment_ = TextAlign::None;
    return 2;
  }

  FontStyle style;
  if (tryGetStyleForward(cmd, &style)) {
    if (processStyle)
      currentInlineStyle_ = style;
    return 2;
  }

  return 0;  // Unknown command
}

// Check if there's a valid ESC token at pos (without modifying state)
size_t FileWordProvider::checkEscTokenAtPos(size_t pos) {
  return parseEscTokenAtPos(pos, nullptr, false);
}

// Parse ESC token when reading BACKWARD - style meanings are inverted
// When going backward through "ESC+B text ESC+b", we encounter ESC+b first (entering bold region)
// and ESC+B second (exiting bold region), so meanings must be swapped
void FileWordProvider::parseEscTokenBackward(size_t pos) {
  if (pos + 1 >= fileSize_)
    return;

  char c = charAt(pos);
  if (c != ESC_CHAR)
    return;

  char cmd = charAt(pos + 1);

  TextAlign align;
  // Backward scanning: encountering a lowercase end token means we ENTER a paragraph alignment region
  if (tryGetAlignmentEnd(cmd, &align)) {
    currentParagraphAlignment_ = align;
    return;
  }
  // Backward scanning: encountering an uppercase start token means we EXIT an alignment region
  if (tryGetAlignmentStart(cmd, &align)) {
    currentParagraphAlignment_ = TextAlign::None;
    return;
  }

  FontStyle style;
  if (tryGetStyleBackward(cmd, &style)) {
    currentInlineStyle_ = style;
  }
}

// Check if we're at the end of an ESC token (at the command byte position)
// Returns true and sets tokenStart if found
bool FileWordProvider::isAtEscTokenEnd(size_t pos, size_t& tokenStart) {
  if (pos == 0)
    return false;

  // Check if previous char is ESC
  char prevChar = charAt(pos - 1);
  if (prevChar != ESC_CHAR)
    return false;

  char cmd = charAt(pos);
  if (isEscCommandChar(cmd)) {
    tokenStart = pos - 1;
    return true;
  }

  return false;
}

StyledWord FileWordProvider::getNextWord() {
  prevIndex_ = index_;

  if (index_ >= fileSize_) {
    return StyledWord();
  }

  // Skip any ESC tokens at current position first
  while (index_ < fileSize_) {
    size_t tokenLen = parseEscTokenAtPos(index_);
    if (tokenLen == 0)
      break;
    index_ += tokenLen;
  }

  if (index_ >= fileSize_) {
    return StyledWord();
  }

  // Skip carriage returns
  while (index_ < fileSize_ && charAt(index_) == '\r') {
    index_++;
  }

  if (index_ >= fileSize_) {
    return StyledWord();
  }

  // Capture style BEFORE reading the word content
  // This ensures the word gets the style that was active at its start,
  // not any style that might be set by ESC tokens encountered during word building
  FontStyle styleForWord = currentInlineStyle_;

  char c = charAt(index_);
  String token;

  // Case 1: Space - read just the space and stop
  if (c == ' ') {
    token += c;
    index_++;
  }
  // Case 2: Single character tokens (newline, tab) - read just that character
  else if (c == '\n' || c == '\t') {
    token += c;
    index_++;
    // Newline resets paragraph alignment
    if (c == '\n') {
      currentParagraphAlignment_ = TextAlign::None;
    }
  }
  // Case 3: Regular character - continue until boundary
  else {
    while (index_ < fileSize_) {
      // Check for ESC token - use checkEscTokenAtPos to detect without modifying state
      size_t tokenLen = checkEscTokenAtPos(index_);
      if (tokenLen > 0) {
        // ESC token marks word boundary - stop here without processing the token
        // The token will be processed on the next getNextWord() call
        break;
      }

      char cc = charAt(index_);
      // Skip carriage returns
      if (cc == '\r') {
        index_++;
        continue;
      }
      // Stop at space or whitespace boundaries
      if (cc == ' ' || cc == '\n' || cc == '\t') {
        break;
      }
      token += cc;
      index_++;
    }
  }

  // // DEBUG: print returned word with alignment
  // {
  //   // Alignment is updated by parseEscTokenAtPos while skipping ESC tokens.
  //   TextAlign align = getParagraphAlignment();
  //   printf("getNextWord returning pos=%d token='%s' style=%d align=%d\n", getCurrentIndex(), token.c_str(),
  //          (int)styleForWord, (int)align);
  // }
  return StyledWord(token, styleForWord);
}

StyledWord FileWordProvider::getPrevWord() {
  prevIndex_ = index_;

  if (index_ == 0) {
    return StyledWord();
  }

  // Move to just before current position
  index_--;

  // Skip backward over ESC tokens (fixed 2-byte format makes this simple)
  // Don't try to invert token meanings - just skip over them
  while (true) {
    // Check if we're at command byte of an ESC token
    if (index_ > 0) {
      size_t tokenStart;
      if (isAtEscTokenEnd(index_, tokenStart)) {
        // Process token backward to update inline styles and paragraph alignment
        parseEscTokenBackward(tokenStart);
        // We're at command byte, skip back over the whole token
        if (tokenStart == 0) {
          // ESC token starts at position 0, nothing before it
          index_ = 0;
          return StyledWord();
        }
        index_ = tokenStart - 1;
        continue;
      }
    }

    // Check if we landed on ESC char itself (start of a token)
    if (charAt(index_) == ESC_CHAR) {
      // Check if valid token without modifying state
      size_t tokenLen = checkEscTokenAtPos(index_);
      if (tokenLen > 0) {
        // Process backward so we update style/alignment context
        parseEscTokenBackward(index_);
        if (index_ == 0) {
          // At start of file, nothing before this token
          return StyledWord();
        }
        index_--;
        continue;
      }
    }

    break;
  }

  // Skip backward over carriage returns
  while (index_ > 0 && charAt(index_) == '\r') {
    index_--;
  }

  // If we ended up at position 0 with an ESC char, we're at start of file with only tokens
  if (charAt(index_) == ESC_CHAR) {
    size_t tokenLen = checkEscTokenAtPos(index_);
    if (tokenLen > 0) {
      // Process the token backward before returning
      parseEscTokenBackward(index_);
      index_ = 0;
      return StyledWord();
    }
  }

  if (index_ >= fileSize_) {
    index_ = 0;
    return StyledWord();
  }

  char c = charAt(index_);
  String token;
  size_t tokenStart = index_;

  // Case 1: Space
  if (c == ' ') {
    token += c;
  }
  // Case 2: Single character tokens
  else if (c == '\n' || c == '\t') {
    token += c;
    if (c == '\n') {
      currentParagraphAlignment_ = TextAlign::None;
    }
  }
  // Case 3: Regular word - find start
  else {
    // Find word start by scanning backward
    while (tokenStart > 0) {
      char prevChar = charAt(tokenStart - 1);
      // Stop at whitespace
      if (prevChar == ' ' || prevChar == '\n' || prevChar == '\t' || prevChar == '\r') {
        break;
      }
      // Stop at ESC token boundary - check if prev char is a command byte with ESC before it
      if (tokenStart >= 2) {
        size_t possibleTokenStart;
        if (isAtEscTokenEnd(tokenStart - 1, possibleTokenStart)) {
          break;
        }
      }
      // Stop if prev char is ESC (we're right after an ESC token)
      if (prevChar == ESC_CHAR) {
        break;
      }
      tokenStart--;
    }

    // Build word from start to current position
    // If token starts at file start but file begins with a UTF-8 BOM, skip the BOM
    if (tokenStart == 0 && hasUtf8BomAtStart()) {
      tokenStart = 3;
    }

    for (size_t i = tokenStart; i <= index_; i++) {
      char cc = charAt(i);
      if (cc != '\r') {
        token += cc;
      }
    }

    index_ = tokenStart;
  }

  // Use restoreStyleContext to correctly compute the style at the word's START position
  // This handles complex style transitions (e.g., B->X->I) correctly by scanning
  // forward from paragraph start to index_ (which is now the word start)
  restoreStyleContext();
  FontStyle styleForWord = currentInlineStyle_;

  // // DEBUG: print returned word with alignment
  // {
  //   // For prevWord, index_ is the start of word; alignment is updated by parseEscTokenBackward
  //   TextAlign align = getParagraphAlignment();
  //   printf("getPrevWord returning pos=%d token='%s' style=%d align=%d\n", getCurrentIndex(), token.c_str(),
  //          (int)styleForWord, (int)align);
  // }
  return StyledWord(token, styleForWord);
}

StyledWord FileWordProvider::scanWord(int direction) {
  // Legacy function - redirect to new implementations
  if (direction == 1) {
    return getNextWord();
  } else {
    return getPrevWord();
  }
}

float FileWordProvider::getPercentage() {
  if (fileSize_ == 0)
    return 1.0f;
  return static_cast<float>(index_) / static_cast<float>(fileSize_);
}

float FileWordProvider::getPercentage(int index) {
  if (fileSize_ == 0)
    return 1.0f;
  return static_cast<float>(index) / static_cast<float>(fileSize_);
}

int FileWordProvider::getCurrentIndex() {
  return (int)index_;
}

char FileWordProvider::peekChar(int offset) {
  long pos = (long)index_ + offset;
  if (pos < 0 || pos >= (long)fileSize_) {
    return '\0';
  }
  return charAt((size_t)pos);
}

int FileWordProvider::consumeChars(int n) {
  if (n <= 0) {
    return 0;
  }

  int consumed = 0;
  while (consumed < n && index_ < fileSize_) {
    // Check for ESC token first
    size_t tokenLen = parseEscTokenAtPos(index_);
    if (tokenLen > 0) {
      // Skip ESC token without counting as consumed
      index_ += tokenLen;
      continue;
    }

    char c = charAt(index_);
    index_++;
    // Skip carriage returns, they don't count as consumed characters
    if (c != '\r') {
      consumed++;
    }
  }
  return consumed;
}

// Helper method to determine if a character is a word boundary
bool FileWordProvider::isWordBoundary(char c) {
  return (c == ' ' || c == '\n' || c == '\t' || c == '\r' || c == ESC_CHAR);
}

bool FileWordProvider::isInsideWord() {
  if (index_ <= 0 || index_ >= fileSize_) {
    return false;
  }

  // Helper lambda to check if a character is a word character (not whitespace/control)
  auto isWordChar = [](char c) { return c != '\0' && c != ' ' && c != '\n' && c != '\t' && c != '\r'; };

  // Check character before current position
  char prevChar = charAt(index_ - 1);
  // Check character at current position
  char currentChar = charAt(index_);

  return isWordChar(prevChar) && isWordChar(currentChar);
}

void FileWordProvider::ungetWord() {
  index_ = prevIndex_;
}

void FileWordProvider::setPosition(int index) {
  if (index < 0)
    index = 0;
  if ((size_t)index > fileSize_)
    index = (int)fileSize_;
  index_ = (size_t)index;
  prevIndex_ = index_;
  // Restore style context for the new position
  restoreStyleContext();
  // If user set to start of file, skip UTF-8 BOM if present BEFORE computing alignment
  if (index_ == 0) {
    skipUtf8BomIfPresent();
  }
  // Recompute paragraph alignment for new position
  computeParagraphAlignmentForPosition(index_);
  // Don't invalidate cache here - getParagraphAlignment will check if we're still in range
}

void FileWordProvider::reset() {
  index_ = 0;
  prevIndex_ = 0;
  // Paragraph alignment caching removed; nothing to invalidate.
  // Skip UTF-8 BOM on reset
  skipUtf8BomIfPresent();
  computeParagraphAlignmentForPosition(index_);
}

TextAlign FileWordProvider::getParagraphAlignment() {
  // Return the computed paragraph alignment (may be None)
  return currentParagraphAlignment_;
}

void FileWordProvider::findParagraphBoundaries(size_t pos, size_t& outStart, size_t& outEnd) {
  // Paragraphs are delimited by newlines
  // Find start: scan backwards to find newline or beginning of file
  outStart = 0;
  if (pos > 0) {
    for (size_t i = pos; i > 0; --i) {
      if (charAt(i - 1) == '\n') {
        outStart = i;
        break;
      }
    }
  }

  // Find end: scan forwards to find newline or end of file
  outEnd = fileSize_;
  for (size_t i = pos; i < fileSize_; ++i) {
    if (charAt(i) == '\n') {
      outEnd = i + 1;  // Include the newline in this paragraph
      break;
    }
  }
}

void FileWordProvider::computeParagraphAlignmentForPosition(size_t pos) {
  // Default to None (no alignment)
  currentParagraphAlignment_ = TextAlign::None;
  if (fileSize_ == 0)
    return;

  // If pos is beyond size, clamp
  if (pos >= fileSize_)
    pos = fileSize_ - 1;

  // Walk left from current position until we find an ESC alignment token or newline
  size_t p = pos;
  while (true) {
    if (p == 0) {
      // Reached start of file without finding newline or token
      break;
    }
    // Check previous char for newline (paragraph boundary)
    char prev = charAt(p - 1);
    if (prev == '\n') {
      // Found paragraph boundary; no alignment on this line
      currentParagraphAlignment_ = TextAlign::None;
      return;
    }
    // Check if we're at an ESC char
    if (charAt(p - 1) == ESC_CHAR && p < fileSize_) {
      char cmd = charAt(p);
      TextAlign align;
      if (tryGetAlignmentStart(cmd, &align)) {
        currentParagraphAlignment_ = align;
        return;
      }
    }
    // Also check if current position is at ESC char (e.g., when p indexes ESC)
    if (charAt(p) == ESC_CHAR && p + 1 < fileSize_) {
      char cmd = charAt(p + 1);
      TextAlign align;
      if (tryGetAlignmentStart(cmd, &align)) {
        currentParagraphAlignment_ = align;
        return;
      }
    }
    if (p == 0)
      break;
    p--;
  }
  // If we fall out, no alignment found -> None
  currentParagraphAlignment_ = TextAlign::None;
}

size_t FileWordProvider::findEscTokenStart(size_t trailingPos) {
  // For ESC format, token is always 2 bytes: ESC + command
  // If trailingPos is at command byte, start is trailingPos - 1
  if (trailingPos == 0)
    return SIZE_MAX;

  if (charAt(trailingPos - 1) == ESC_CHAR) {
    return trailingPos - 1;
  }
  return SIZE_MAX;
}

void FileWordProvider::restoreStyleContext() {
  // Reset style to default first
  currentInlineStyle_ = FontStyle::REGULAR;

  if (index_ == 0 || fileSize_ == 0)
    return;

  // Find paragraph start (newline boundary)
  size_t paraStart = 0;
  for (size_t i = index_; i > 0; --i) {
    if (charAt(i - 1) == '\n') {
      paraStart = i;
      break;
    }
  }

  // Scan forward from paragraph start to current position, processing style tokens
  size_t scanPos = paraStart;
  while (scanPos < index_) {
    if (charAt(scanPos) == ESC_CHAR && scanPos + 1 < fileSize_) {
      FontStyle style;
      char cmd = charAt(scanPos + 1);
      if (tryGetStyleForward(cmd, &style)) {
        currentInlineStyle_ = style;
      }
      scanPos += 2;  // Skip ESC token
    } else {
      scanPos++;
    }
  }
}

bool FileWordProvider::hasUtf8BomAtStart() {
  if (fileSize_ < 3 || !file_)
    return false;
  // Make sure we have bytes in buffer
  if (!ensureBufferForPos(0))
    return false;
  return (bufLen_ >= 3 && buf_[0] == 0xEF && buf_[1] == 0xBB && buf_[2] == 0xBF);
}

void FileWordProvider::skipUtf8BomIfPresent() {
  if (hasUtf8BomAtStart() && index_ == 0) {
    index_ = 3;
    prevIndex_ = index_;
  }
}
