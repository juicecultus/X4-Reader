#include "SimpleXmlParser.h"

#include <Arduino.h>

SimpleXmlParser::SimpleXmlParser()
    : bufferStartPos_(0),
      bufferLen_(0),
      filePos_(0),
      currentNodeType_(None),
      isEmptyElement_(false),
      textNodeStartPos_(0),
      textNodeEndPos_(0),
      textNodeCurrentPos_(0),
      peekedTextNodeChar_('\0'),
      hasPeekedTextNodeChar_(false),
      peekedPrevTextNodeChar_('\0'),
      hasPeekedPrevTextNodeChar_(false) {}

SimpleXmlParser::~SimpleXmlParser() {
  close();
}

bool SimpleXmlParser::open(const char* filepath) {
  close();
  file_ = SD.open(filepath, FILE_READ);
  if (!file_) {
    return false;
  }

  bufferStartPos_ = 0;
  bufferLen_ = 0;
  filePos_ = 0;
  textNodeStartPos_ = 0;
  textNodeEndPos_ = 0;
  textNodeCurrentPos_ = 0;
  peekedTextNodeChar_ = '\0';
  hasPeekedTextNodeChar_ = false;
  peekedPrevTextNodeChar_ = '\0';
  hasPeekedPrevTextNodeChar_ = false;

  return true;
}

void SimpleXmlParser::close() {
  if (file_) {
    file_.close();
  }
  bufferStartPos_ = 0;
  bufferLen_ = 0;
  filePos_ = 0;
  textNodeStartPos_ = 0;
  textNodeEndPos_ = 0;
  textNodeCurrentPos_ = 0;
  peekedTextNodeChar_ = '\0';
  hasPeekedTextNodeChar_ = false;
  peekedPrevTextNodeChar_ = '\0';
  hasPeekedPrevTextNodeChar_ = false;
}

// Load buffer centered around the given position
bool SimpleXmlParser::loadBufferAround(size_t pos) {
  if (!file_) {
    return false;
  }

  // Position the buffer to contain pos
  // Try to center the buffer, but respect file boundaries
  size_t fileSize = file_.size();
  if (fileSize == 0) {
    return false;
  }

  // Try to position buffer so pos is in the middle
  size_t idealStart = (pos >= BUFFER_SIZE / 2) ? (pos - BUFFER_SIZE / 2) : 0;

  // Adjust if we'd go past end of file
  if (idealStart + BUFFER_SIZE > fileSize) {
    idealStart = (fileSize > BUFFER_SIZE) ? (fileSize - BUFFER_SIZE) : 0;
  }

  // Seek to position and read
  if (!file_.seek(idealStart)) {
    return false;
  }

  bufferStartPos_ = idealStart;
  bufferLen_ = file_.read(buffer_, BUFFER_SIZE);

  return bufferLen_ > 0;
}

// Get byte at any position in the file
char SimpleXmlParser::getByteAt(size_t pos) {
  if (!file_) {
    return '\0';
  }

  // Check if position is already in buffer
  if (bufferLen_ > 0 && pos >= bufferStartPos_ && pos < bufferStartPos_ + bufferLen_) {
    return (char)buffer_[pos - bufferStartPos_];
  }

  // Need to load buffer around this position
  if (!loadBufferAround(pos)) {
    return '\0';
  }

  // Check if position is now in buffer
  if (pos >= bufferStartPos_ && pos < bufferStartPos_ + bufferLen_) {
    return (char)buffer_[pos - bufferStartPos_];
  }

  return '\0';
}

char SimpleXmlParser::peekChar() {
  return getByteAt(filePos_);
}

char SimpleXmlParser::readChar() {
  char c = getByteAt(filePos_);
  if (c != '\0') {
    filePos_++;
  }
  return c;
}

bool SimpleXmlParser::skipWhitespace() {
  while (true) {
    char c = peekChar();
    if (c == '\0') {
      return false;
    }
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
      readChar();
    } else {
      return true;
    }
  }
}

bool SimpleXmlParser::matchString(const char* str) {
  size_t len = strlen(str);

  // Save current position for potential rewind
  size_t savedFilePos = filePos_;

  // Try to match all characters
  for (size_t i = 0; i < len; i++) {
    char c = readChar();
    if (c != str[i]) {
      // Rewind to saved position
      filePos_ = savedFilePos;
      return false;
    }
  }
  return true;
}

// ========== XmlReader-style API Implementation ==========

bool SimpleXmlParser::read() {
  if (!file_) {
    currentNodeType_ = EndOfFile;
    return false;
  }

  // If we just read a text node and haven't consumed it, skip to the next '<'
  if (currentNodeType_ == Text && textNodeCurrentPos_ > 0) {
    // Position at where text reading left off
    filePos_ = textNodeCurrentPos_;

    // Skip until we find '<' or EOF
    while (true) {
      char c = peekChar();
      if (c == '\0' || c == '<') {
        break;
      }
      readChar();
    }
  }

  // Clear previous state
  currentName_ = "";
  currentValue_ = "";
  isEmptyElement_ = false;
  attributes_.clear();
  textNodeStartPos_ = 0;
  textNodeEndPos_ = 0;
  textNodeCurrentPos_ = 0;
  peekedTextNodeChar_ = '\0';
  hasPeekedTextNodeChar_ = false;
  hasPeekedPrevTextNodeChar_ = false;

  // Check what's next
  while (true) {
    char c = peekChar();
    if (c == '\0') {
      currentNodeType_ = EndOfFile;
      return false;
    }

    if (c == '<') {
      readChar();  // consume '<'
      char next = peekChar();

      if (next == '/') {
        // End element
        return readEndElement();
      } else if (next == '!') {
        readChar();  // consume '!'
        char peek2 = peekChar();
        if (peek2 == '-') {
          return readComment();
        } else if (peek2 == '[') {
          return readCDATA();
        }
        // Unknown, skip
        skipToEndOfTag();
        continue;
      } else if (next == '?') {
        return readProcessingInstruction();
      } else {
        // Start element
        return readElement();
      }
    } else {
      // Text node
      return readText();
    }
  }

  return false;
}

bool SimpleXmlParser::readElement() {
  currentNodeType_ = Element;
  currentName_ = readElementName();

  // Parse attributes
  parseAttributes();

  // Check for self-closing tag
  skipWhitespace();
  char c = peekChar();
  if (c == '/') {
    readChar();  // consume '/'
    isEmptyElement_ = true;
  }

  // Skip to end of tag
  while (true) {
    c = readChar();
    if (c == '>' || c == '\0')
      break;
  }

  return true;
}

bool SimpleXmlParser::readEndElement() {
  currentNodeType_ = EndElement;
  readChar();  // consume '/'
  currentName_ = readElementName();

  // Skip to '>'
  while (true) {
    char c = readChar();
    if (c == '>' || c == '\0')
      break;
  }

  return true;
}

bool SimpleXmlParser::readText() {
  currentNodeType_ = Text;

  // Record start position of text node (right after '>')
  textNodeStartPos_ = filePos_;
  textNodeCurrentPos_ = filePos_;

  // Scan ahead to find the end of text (the next '<')
  // While scanning, also check if it's whitespace-only
  size_t scanPos = filePos_;
  bool hasNonWhitespace = false;

  while (true) {
    char c = getByteAt(scanPos);
    if (c == '\0' || c == '<') {
      break;
    }
    // Check if this character is not whitespace
    if (!hasNonWhitespace && c != ' ' && c != '\t' && c != '\n' && c != '\r') {
      hasNonWhitespace = true;
    }
    scanPos++;
  }

  // Set end position to where we found '<' (no trimming)
  textNodeEndPos_ = scanPos;

  // Skip whitespace-only text nodes automatically
  if (!hasNonWhitespace) {
    // Move position past this text node and read the next node
    filePos_ = textNodeEndPos_;
    return read();
  }

  return true;
}

bool SimpleXmlParser::readComment() {
  currentNodeType_ = Comment;
  currentValue_ = "";

  // Expect "<!--"
  if (readChar() != '-' || peekChar() != '-') {
    skipToEndOfTag();
    return false;
  }
  readChar();  // consume second '-'

  // Read until "-->"
  while (true) {
    char c = readChar();
    if (c == '\0')
      break;

    if (c == '-' && peekChar() == '-') {
      readChar();  // consume second '-'
      if (peekChar() == '>') {
        readChar();  // consume '>'
        break;
      }
      currentValue_ += '-';
      currentValue_ += '-';
    } else {
      currentValue_ += c;
    }
  }

  return true;
}

bool SimpleXmlParser::readCDATA() {
  currentNodeType_ = CDATA;
  currentValue_ = "";

  // Expect "<![CDATA["
  if (matchString("[CDATA[")) {
    // Read until "]]>"
    while (true) {
      char c = readChar();
      if (c == '\0')
        break;

      if (c == ']' && peekChar() == ']') {
        readChar();  // consume second ']'
        if (peekChar() == '>') {
          readChar();  // consume '>'
          break;
        }
        currentValue_ += ']';
        currentValue_ += ']';
      } else {
        currentValue_ += c;
      }
    }
  }

  return true;
}

bool SimpleXmlParser::readProcessingInstruction() {
  currentNodeType_ = ProcessingInstruction;
  readChar();  // consume '?'

  currentName_ = readElementName();
  currentValue_ = "";

  // Read until "?>"
  while (true) {
    char c = readChar();
    if (c == '\0')
      break;

    if (c == '?' && peekChar() == '>') {
      readChar();  // consume '>'
      break;
    }

    currentValue_ += c;
  }

  return true;
}

String SimpleXmlParser::readElementName() {
  String name;

  while (true) {
    char c = peekChar();
    if (c == '\0' || c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '>' || c == '/' || c == '=')
      break;

    name += readChar();
  }

  return name;
}

void SimpleXmlParser::parseAttributes() {
  attributes_.clear();

  while (true) {
    skipWhitespace();
    char c = peekChar();

    // Check for end of tag
    if (c == '>' || c == '/' || c == '\0')
      break;

    // Read attribute name
    String attrName = readElementName();
    if (attrName.isEmpty())
      break;

    skipWhitespace();

    // Expect '='
    if (peekChar() != '=')
      break;
    readChar();  // consume '='

    skipWhitespace();

    // Read attribute value (quoted)
    char quote = peekChar();
    if (quote != '"' && quote != '\'')
      break;
    readChar();  // consume quote

    String attrValue;
    while (true) {
      char c = readChar();
      if (c == '\0' || c == quote)
        break;
      attrValue += c;
    }

    Attribute attr;
    attr.name = attrName;
    attr.value = attrValue;
    attributes_.push_back(attr);
  }
}

void SimpleXmlParser::skipToEndOfTag() {
  while (true) {
    char c = readChar();
    if (c == '>' || c == '\0')
      break;
  }
}

String SimpleXmlParser::getAttribute(const char* name) const {
  size_t nameLen = strlen(name);

  for (size_t i = 0; i < attributes_.size(); i++) {
    const String& attrName = attributes_[i].name;
    if (attrName.length() != nameLen)
      continue;

    // Case-insensitive comparison
    bool match = true;
    for (size_t j = 0; j < nameLen; j++) {
      char c1 = attrName.charAt(j);
      char c2 = name[j];
      if (c1 >= 'A' && c1 <= 'Z')
        c1 += 32;  // tolower
      if (c2 >= 'A' && c2 <= 'Z')
        c2 += 32;  // tolower
      if (c1 != c2) {
        match = false;
        break;
      }
    }

    if (match) {
      return attributes_[i].value;
    }
  }
  return String("");
}

char SimpleXmlParser::readTextNodeCharForward() {
  if (currentNodeType_ != Text) {
    return '\0';
  }

  // Clear any peeked character
  hasPeekedTextNodeChar_ = false;

  // Check if we've reached the end of text
  if (textNodeEndPos_ > 0 && textNodeCurrentPos_ >= textNodeEndPos_) {
    return '\0';
  }

  // Check if we've reached '<' (end of text)
  char c = getByteAt(textNodeCurrentPos_);
  if (c == '\0' || c == '<') {
    return '\0';
  }

  // Move position forward
  textNodeCurrentPos_++;
  filePos_ = textNodeCurrentPos_;

  return c;
}

char SimpleXmlParser::readTextNodeCharBackward() {
  if (currentNodeType_ != Text) {
    return '\0';
  }

  // Clear any peeked character
  hasPeekedTextNodeChar_ = false;

  // Check if we've reached the beginning
  if (textNodeCurrentPos_ <= textNodeStartPos_) {
    return '\0';
  }

  // Move to previous position
  textNodeCurrentPos_--;
  filePos_ = textNodeCurrentPos_;

  // Read the character
  char c = getByteAt(textNodeCurrentPos_);
  if (c == '\0') {
    textNodeCurrentPos_++;  // Restore position
    return '\0';
  }

  return c;
}

char SimpleXmlParser::peekTextNodeChar() {
  if (currentNodeType_ != Text) {
    return '\0';
  }

  if (hasPeekedTextNodeChar_) {
    return peekedTextNodeChar_;
  }

  // Peeking forward
  peekedTextNodeChar_ = getByteAt(textNodeCurrentPos_);

  // Check if we've reached '<' (end of text)
  if (peekedTextNodeChar_ == '<' || peekedTextNodeChar_ == '\0') {
    return '\0';
  }

  hasPeekedTextNodeChar_ = true;

  return peekedTextNodeChar_;
}

bool SimpleXmlParser::hasMoreTextChars() const {
  if (currentNodeType_ != Text) {
    return false;
  }

  // Check if we've reached the end of text node (same check as readTextNodeCharForward)
  if (textNodeEndPos_ > 0 && textNodeCurrentPos_ >= textNodeEndPos_) {
    return false;
  }

  // Reading forward - check if next char is '<'
  char c = const_cast<SimpleXmlParser*>(this)->getByteAt(textNodeCurrentPos_);
  return c != '\0' && c != '<';
}

bool SimpleXmlParser::readBackward() {
  if (!file_) {
    currentNodeType_ = EndOfFile;
    return false;
  }

  // Clear previous state
  currentName_ = "";
  currentValue_ = "";
  isEmptyElement_ = false;
  attributes_.clear();
  textNodeStartPos_ = 0;
  textNodeEndPos_ = 0;
  textNodeCurrentPos_ = 0;
  peekedTextNodeChar_ = '\0';
  hasPeekedTextNodeChar_ = false;
  hasPeekedPrevTextNodeChar_ = false;

  // If we're at the beginning, we've reached the end of backward reading
  if (filePos_ == 0) {
    currentNodeType_ = EndOfFile;
    return false;
  }

  // Start scanning backward from current position
  size_t originalPos = filePos_;  // Save original position
  size_t scanPos = filePos_ - 1;

  char c = getByteAt(scanPos);

  // If we're at '>', this is the end of a tag - find its start and parse it
  if (c == '>') {
    // We're at the end of a tag - scan backward to find the opening '<'
    size_t tagEnd = scanPos;
    size_t tagStart = scanPos;

    while (tagStart > 0) {
      tagStart--;
      char tagChar = getByteAt(tagStart);
      if (tagChar == '<') {
        break;
      }
    }

    // Skip declarations like <!DOCTYPE ...> which would otherwise return the next element twice
    char tagLead = getByteAt(tagStart + 1);
    if (tagLead == '!') {
      filePos_ = tagStart;
      if (tagStart == 0) {
        currentNodeType_ = EndOfFile;
        return false;
      }
      return readBackward();
    }

    // Now parse the tag starting at tagStart
    filePos_ = tagStart;

    // Read and parse this tag using forward parsing
    bool result = read();

    // After reading, set filePos_ to start of this tag for next backward read
    // The next readBackward() will process content that ends right before this tag
    filePos_ = tagStart;

    return result;

  } else if (c == '<') {
    // We're at '<' which means we're at the start of a tag
    // This shouldn't happen in normal backward reading since we should hit '>' first
    // But handle it: scan forward to find the '>' then come back and parse
    size_t tagEnd = scanPos;
    while (tagEnd < file_.size()) {
      char ch = getByteAt(tagEnd);
      if (ch == '>') {
        break;
      }
      tagEnd++;
    }

    // Now parse from scanPos (the '<')
    filePos_ = scanPos;

    bool result = read();

    // After reading, restore position for next backward read
    filePos_ = scanPos;
    return result;

  } else {
    // We're in text content
    // Scan backward to find where this text starts (right after previous '>')
    size_t textEnd = originalPos;  // The original position before we started scanning
    size_t searchPos = scanPos;
    size_t textStart = 0;

    while (searchPos > 0) {
      searchPos--;
      char ch = getByteAt(searchPos);

      if (ch == '>') {
        // Found end of previous tag - text starts right after this
        textStart = searchPos + 1;
        break;
      }
      if (ch == '<') {
        // Found start of a tag - we must have been inside a tag, not text
        // This means the original character wasn't actually text
        // Parse this tag instead
        filePos_ = searchPos;

        bool result = read();

        // After reading, restore position for next backward read
        filePos_ = searchPos;
        return result;
      }
    }

    // Set up text node and check if it's whitespace-only
    bool hasNonWhitespace = false;
    for (size_t pos = textStart; pos < textEnd; pos++) {
      char ch = getByteAt(pos);
      if (ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r') {
        hasNonWhitespace = true;
        break;
      }
    }

    // Skip whitespace-only text nodes automatically
    if (!hasNonWhitespace) {
      // Move position before this text node and read the previous node
      if (textStart == 0) {
        // At beginning of file with whitespace-only text
        currentNodeType_ = EndOfFile;
        return false;
      }
      filePos_ = textStart - 1;
      return readBackward();
    }

    currentNodeType_ = Text;
    textNodeStartPos_ = textStart;
    textNodeEndPos_ = textEnd;
    // When reading backward, position at end of text for backward character reading
    textNodeCurrentPos_ = textEnd;

    // Set filePos_ to where the text starts for next backward read
    // (The next scanPos will be textStart - 1, which is the '>' before the text)
    filePos_ = textStart;

    return true;
  }

  // Reached beginning of file
  currentNodeType_ = EndOfFile;
  return false;
}

bool SimpleXmlParser::hasMoreTextCharsBackward() const {
  if (currentNodeType_ != Text) {
    return false;
  }
  return textNodeCurrentPos_ > textNodeStartPos_;
}

char SimpleXmlParser::peekPrevTextNodeChar() {
  if (currentNodeType_ != Text) {
    return '\0';
  }
  if (hasPeekedPrevTextNodeChar_) {
    return peekedPrevTextNodeChar_;
  }
  if (textNodeCurrentPos_ <= textNodeStartPos_) {
    return '\0';
  }
  // Get char before current
  size_t pos = textNodeCurrentPos_ - 1;
  peekedPrevTextNodeChar_ = getByteAt(pos);
  hasPeekedPrevTextNodeChar_ = true;
  return peekedPrevTextNodeChar_;
}

char SimpleXmlParser::readPrevTextNodeChar() {
  if (currentNodeType_ != Text) {
    return '\0';
  }
  // Clear any peeked prev char
  hasPeekedPrevTextNodeChar_ = false;

  if (textNodeCurrentPos_ <= textNodeStartPos_) {
    return '\0';
  }

  // Move to one char before current and read it
  size_t pos = textNodeCurrentPos_ - 1;
  char c = getByteAt(pos);
  textNodeCurrentPos_ = pos;
  return c;
}

SimpleXmlParser::Position SimpleXmlParser::getPosition() const {
  Position pos;
  pos.nodeType = currentNodeType_;
  pos.name = currentName_;
  pos.value = currentValue_;
  pos.isEmpty = isEmptyElement_;
  pos.textStart = textNodeStartPos_;
  pos.textEnd = textNodeEndPos_;
  pos.textCurrent = (currentNodeType_ == Text) ? textNodeCurrentPos_ : 0;
  pos.filePos = (currentNodeType_ == Text) ? textNodeCurrentPos_ : filePos_;

  for (const auto& attr : attributes_) {
    pos.attributes.push_back({attr.name, attr.value});
  }

  return pos;
}

bool SimpleXmlParser::setPosition(const Position& position) {
  if (!file_ || position.filePos > file_.size()) {
    return false;
  }

  const bool preferText =
      position.textCurrent > 0 || position.nodeType == Text || position.filePos == position.textCurrent;

  // First, move to the requested file position. This determines whether we're inside a
  // text node or between tags without relying on any cached node metadata.
  if (!seekToFilePosition(position.filePos, preferText)) {
    return false;
  }

  // When inside a text node, ensure the current pointer is restored. All other metadata can
  // be reconstructed by reading from the file contents when needed.
  if (currentNodeType_ == Text) {
    if (position.textCurrent >= textNodeStartPos_ && position.textCurrent <= textNodeEndPos_) {
      textNodeCurrentPos_ = position.textCurrent;
      filePos_ = textNodeCurrentPos_;
    }
    hasPeekedTextNodeChar_ = false;
    peekedTextNodeChar_ = '\0';
    hasPeekedPrevTextNodeChar_ = false;
    peekedPrevTextNodeChar_ = '\0';
    loadBufferAround(filePos_);
    return true;
  }

  // For non-text nodes, prefer parsing forward from the nearest tag boundary to rebuild
  // the exact node at this position. This avoids ambiguity when the saved file position
  // falls inside a tag rather than exactly at the '<'.
  hasPeekedTextNodeChar_ = false;
  peekedTextNodeChar_ = '\0';
  hasPeekedPrevTextNodeChar_ = false;
  peekedPrevTextNodeChar_ = '\0';

  // If we're at the beginning of the file, just read forward.
  if (position.filePos == 0) {
    filePos_ = 0;
    return read();
  }

  if (position.nodeType == Element || position.nodeType == EndElement || position.nodeType == Comment ||
      position.nodeType == ProcessingInstruction || position.nodeType == CDATA) {
    size_t tagStart = position.filePos;
    while (tagStart > 0 && getByteAt(tagStart) != '<') {
      tagStart--;
    }
    filePos_ = tagStart;
    return read();
  }

  filePos_ = position.filePos;
  if (!readBackward()) {
    return false;
  }

  // If the restored position is before the requested offset, advance forward until we
  // reach the node that covers or follows the target location.
  while (getFilePosition() < position.filePos) {
    if (!read()) {
      break;
    }
  }

  return true;
}

bool SimpleXmlParser::seekToFilePosition(size_t pos, bool preferTextBoundary) {
  if (!file_) {
    return false;
  }

  // Validate position
  if (pos > file_.size()) {
    return false;
  }

  // Special case: seeking to end of file
  if (pos == file_.size()) {
    filePos_ = pos;
    currentNodeType_ = None;
    currentName_ = "";
    isEmptyElement_ = false;
    attributes_.clear();
    textNodeStartPos_ = 0;
    textNodeEndPos_ = 0;
    textNodeCurrentPos_ = 0;
    hasPeekedTextNodeChar_ = false;
    hasPeekedPrevTextNodeChar_ = false;
    return true;
  }

  // Check if position is inside a text node (between '>' and '<')
  // Scan backward to find the nearest '>' or '<'
  size_t scanBack = pos;
  bool foundTextStart = false;
  size_t textStart = 0;

  while (scanBack > 0) {
    scanBack--;
    char c = getByteAt(scanBack);
    if (c == '>') {
      // Found end of previous tag - pos is in text content after this tag
      textStart = scanBack + 1;
      foundTextStart = true;
      break;
    }
    if (c == '<') {
      // Found start of a tag - pos is inside a tag, not text
      // Just set file position and let read() handle it
      filePos_ = pos;
      currentNodeType_ = None;
      currentName_ = "";
      isEmptyElement_ = false;
      attributes_.clear();
      textNodeStartPos_ = 0;
      textNodeEndPos_ = 0;
      textNodeCurrentPos_ = 0;
      hasPeekedTextNodeChar_ = false;
      hasPeekedPrevTextNodeChar_ = false;
      return true;
    }
  }

  // If we're at position 0 and didn't find '>', we're at the start
  if (!foundTextStart && scanBack == 0) {
    // Check if first char is '<' (tag) or text
    if (getByteAt(0) == '<') {
      filePos_ = pos;
      currentNodeType_ = None;
      currentName_ = "";
      isEmptyElement_ = false;
      attributes_.clear();
      textNodeStartPos_ = 0;
      textNodeEndPos_ = 0;
      textNodeCurrentPos_ = 0;
      hasPeekedTextNodeChar_ = false;
      hasPeekedPrevTextNodeChar_ = false;
      return true;
    }
    textStart = 0;
    foundTextStart = true;
  }

  if (foundTextStart) {
    // Scan forward to find where text ends (the next '<')
    size_t textEnd = pos;
    while (textEnd < file_.size()) {
      char c = getByteAt(textEnd);
      if (c == '<') {
        break;
      }
      textEnd++;
    }

    // Check if this is actual text content (has non-whitespace)
    // Also check if we're actually inside the text (not at the boundary)
    bool hasNonWhitespace = false;
    for (size_t i = textStart; i < textEnd; i++) {
      char c = getByteAt(i);
      if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
        hasNonWhitespace = true;
        break;
      }
    }

    // Only set up as text node if:
    // 1. There's actual content (non-whitespace)
    // 2. The seek position is actually inside the text, not at the boundary
    if (hasNonWhitespace && pos < textEnd && pos >= textStart) {
      // Set up as a text node - this allows reading characters directly
      const bool atBoundary = pos == textStart;
      if (!atBoundary || preferTextBoundary) {
        currentNodeType_ = Text;
        textNodeStartPos_ = textStart;
        textNodeEndPos_ = textEnd;
        textNodeCurrentPos_ = pos;
        filePos_ = pos;
        currentName_ = "";
        isEmptyElement_ = false;
        attributes_.clear();
        hasPeekedTextNodeChar_ = false;
        hasPeekedPrevTextNodeChar_ = false;
        return true;
      }
    }
  }

  // Default: just set file position, let read() handle parsing
  filePos_ = pos;
  currentNodeType_ = None;
  currentName_ = "";
  isEmptyElement_ = false;
  attributes_.clear();
  textNodeStartPos_ = 0;
  textNodeEndPos_ = 0;
  textNodeCurrentPos_ = 0;
  hasPeekedTextNodeChar_ = false;
  hasPeekedPrevTextNodeChar_ = false;

  return true;
}
void SimpleXmlParser::restoreState(size_t pos, NodeType nodeType, size_t textStart, size_t textEnd,
                                   const String& elementName, bool isEmpty) {
  Position position;
  position.filePos = (nodeType == Text) ? pos : pos;
  position.nodeType = nodeType;
  position.name = elementName;
  position.isEmpty = isEmpty;
  position.textStart = textStart;
  position.textEnd = textEnd;
  position.textCurrent = (nodeType == Text) ? pos : 0;
  setPosition(position);
}
