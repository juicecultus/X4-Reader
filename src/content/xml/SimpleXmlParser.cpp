#include "SimpleXmlParser.h"

#include <Arduino.h>

SimpleXmlParser::SimpleXmlParser()
    : buffer_(nullptr),
      memoryData_(nullptr),
      memorySize_(0),
      usingMemory_(false),
      streamCallback_(nullptr),
      streamUserData_(nullptr),
      usingStream_(false),
      streamPosition_(0),
      streamEOF_(false),
      bufferStartPos_(0),
      bufferLen_(0),
      filePos_(0),
      streamCurrentBuffer_(-1),
      currentNodeType_(None),
      isEmptyElement_(false),
      textNodeStartPos_(0),
      textNodeEndPos_(0),
      textNodeCurrentPos_(0),
      peekedTextNodeChar_('\0'),
      hasPeekedTextNodeChar_(false),
      streamTextBufferPos_(0),
      elementStartPos_(0),
      elementEndPos_(0) {
  // Allocate primary buffer on heap to avoid stack overflow on ESP32
  buffer_ = (uint8_t*)malloc(BUFFER_SIZE);

  // Initialize streaming buffers to null
  for (size_t i = 0; i < NUM_STREAM_BUFFERS; i++) {
    streamBuffers_[i] = nullptr;
    streamBufferStarts_[i] = 0;
    streamBufferLengths_[i] = 0;
  }
}

SimpleXmlParser::~SimpleXmlParser() {
  close();
  // Free primary buffer
  if (buffer_) {
    free(buffer_);
    buffer_ = nullptr;
  }
}

bool SimpleXmlParser::open(const char* filepath) {
  close();

  // Check if primary buffer allocation succeeded
  if (!buffer_) {
    return false;
  }

  file_ = SD.open(filepath, FILE_READ);
  if (!file_) {
    return false;
  }

  usingMemory_ = false;
  bufferStartPos_ = 0;
  bufferLen_ = 0;
  filePos_ = 0;
  currentNodeType_ = None;
  textNodeStartPos_ = 0;
  textNodeEndPos_ = 0;
  textNodeCurrentPos_ = 0;
  peekedTextNodeChar_ = '\0';
  hasPeekedTextNodeChar_ = false;
  elementStartPos_ = 0;
  elementEndPos_ = 0;

  return true;
}

bool SimpleXmlParser::openFromMemory(const char* data, size_t dataSize) {
  close();

  // Check if primary buffer allocation succeeded
  if (!buffer_) {
    return false;
  }

  memoryData_ = data;
  memorySize_ = dataSize;
  usingMemory_ = true;

  bufferStartPos_ = 0;
  bufferLen_ = 0;
  filePos_ = 0;
  currentNodeType_ = None;
  textNodeStartPos_ = 0;
  textNodeEndPos_ = 0;
  textNodeCurrentPos_ = 0;
  peekedTextNodeChar_ = '\0';
  hasPeekedTextNodeChar_ = false;
  elementStartPos_ = 0;
  elementEndPos_ = 0;

  return true;
}

bool SimpleXmlParser::openFromStream(StreamCallback callback, void* userData) {
  close();

  if (!callback) {
    return false;
  }

  streamCallback_ = callback;
  streamUserData_ = userData;
  usingStream_ = true;
  streamPosition_ = 0;
  streamEOF_ = false;
  streamCurrentBuffer_ = -1;

  // Allocate sliding window buffers
  for (size_t i = 0; i < NUM_STREAM_BUFFERS; i++) {
    streamBuffers_[i] = (uint8_t*)malloc(BUFFER_SIZE);
    if (!streamBuffers_[i]) {
      // Allocation failed, clean up
      for (size_t j = 0; j < i; j++) {
        free(streamBuffers_[j]);
        streamBuffers_[j] = nullptr;
      }
      return false;
    }
    streamBufferStarts_[i] = 0;
    streamBufferLengths_[i] = 0;
  }

  bufferStartPos_ = 0;
  bufferLen_ = 0;
  filePos_ = 0;
  currentNodeType_ = None;
  textNodeStartPos_ = 0;
  textNodeEndPos_ = 0;
  textNodeCurrentPos_ = 0;
  peekedTextNodeChar_ = '\0';
  hasPeekedTextNodeChar_ = false;
  elementStartPos_ = 0;
  elementEndPos_ = 0;

  return true;
}

void SimpleXmlParser::close() {
  if (file_) {
    file_.close();
  }
  memoryData_ = nullptr;
  memorySize_ = 0;
  usingMemory_ = false;
  streamCallback_ = nullptr;
  streamUserData_ = nullptr;

  // Free streaming buffers
  if (usingStream_) {
    for (size_t i = 0; i < NUM_STREAM_BUFFERS; i++) {
      if (streamBuffers_[i]) {
        free(streamBuffers_[i]);
        streamBuffers_[i] = nullptr;
      }
      streamBufferStarts_[i] = 0;
      streamBufferLengths_[i] = 0;
    }
  }

  usingStream_ = false;
  streamPosition_ = 0;
  streamEOF_ = false;
  streamCurrentBuffer_ = -1;
  bufferStartPos_ = 0;
  bufferLen_ = 0;
  filePos_ = 0;
  currentNodeType_ = None;
  textNodeStartPos_ = 0;
  textNodeEndPos_ = 0;
  textNodeCurrentPos_ = 0;
  peekedTextNodeChar_ = '\0';
  hasPeekedTextNodeChar_ = false;
  elementStartPos_ = 0;
  elementEndPos_ = 0;
}

// Load buffer centered around the given position
bool SimpleXmlParser::loadBufferAround(size_t pos) {
  if (usingStream_) {
    // Check if position is already in one of our sliding window buffers
    for (size_t i = 0; i < NUM_STREAM_BUFFERS; i++) {
      size_t bufStart = streamBufferStarts_[i];
      size_t bufEnd = bufStart + streamBufferLengths_[i];
      if (pos >= bufStart && pos < bufEnd) {
        // Found it! Point to this buffer
        bufferStartPos_ = bufStart;
        bufferLen_ = streamBufferLengths_[i];
        // Copy to buffer_ for compatibility with existing getByteAt logic
        memcpy(buffer_, streamBuffers_[i], streamBufferLengths_[i]);
        return true;
      }
    }

    // Position not in any buffer - need to load more data
    // Keep loading until we have the position or hit EOF/error
    while (!streamEOF_) {
      // Find the next buffer to fill (circular)
      int nextBuffer = (streamCurrentBuffer_ + 1) % NUM_STREAM_BUFFERS;

      // Read into this buffer
      int bytesRead = streamCallback_((char*)streamBuffers_[nextBuffer], BUFFER_SIZE, streamUserData_);
      if (bytesRead < 0) {
        return false;  // Error
      }

      if (bytesRead == 0) {
        streamEOF_ = true;
        return false;
      }

      // Update buffer metadata
      streamBufferStarts_[nextBuffer] = streamPosition_;
      streamBufferLengths_[nextBuffer] = bytesRead;
      streamPosition_ += bytesRead;
      streamCurrentBuffer_ = nextBuffer;

      // Check if the requested position is now in this new buffer
      size_t bufStart = streamBufferStarts_[nextBuffer];
      size_t bufEnd = bufStart + streamBufferLengths_[nextBuffer];
      if (pos >= bufStart && pos < bufEnd) {
        // Found it! Point to this buffer
        bufferStartPos_ = bufStart;
        bufferLen_ = streamBufferLengths_[nextBuffer];
        memcpy(buffer_, streamBuffers_[nextBuffer], bufferLen_);
        return true;
      }

      // Position is still ahead, continue loading
    }

    return false;  // EOF reached without finding position
  }

  if (usingMemory_) {
    // For memory-based parsing, just point to the right location
    if (memorySize_ == 0) {
      return false;
    }

    // Try to position buffer so pos is in the middle
    size_t idealStart = (pos >= BUFFER_SIZE / 2) ? (pos - BUFFER_SIZE / 2) : 0;

    // Adjust if we'd go past end
    if (idealStart + BUFFER_SIZE > memorySize_) {
      idealStart = (memorySize_ > BUFFER_SIZE) ? (memorySize_ - BUFFER_SIZE) : 0;
    }

    bufferStartPos_ = idealStart;
    bufferLen_ = (memorySize_ - idealStart > BUFFER_SIZE) ? BUFFER_SIZE : (memorySize_ - idealStart);

    // Copy from memory to buffer
    memcpy(buffer_, memoryData_ + idealStart, bufferLen_);

    return bufferLen_ > 0;
  }

  if (!file_) {
    return false;
  }

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

  if (!file_.seek(idealStart)) {
    return false;
  }

  bufferStartPos_ = idealStart;
  bufferLen_ = file_.read(buffer_, BUFFER_SIZE);

  return bufferLen_ > 0;
}

char SimpleXmlParser::getByteAt(size_t pos) {
  if (usingStream_) {
    // For streaming, we can only access data in the current buffer
    // and we can only move forward
    if (bufferLen_ > 0 && pos >= bufferStartPos_ && pos < bufferStartPos_ + bufferLen_) {
      return (char)buffer_[pos - bufferStartPos_];
    }

    // If position is beyond buffer, need to load more
    if (pos >= bufferStartPos_ + bufferLen_) {
      if (!loadBufferAround(pos)) {
        return '\0';
      }
      if (pos >= bufferStartPos_ && pos < bufferStartPos_ + bufferLen_) {
        return (char)buffer_[pos - bufferStartPos_];
      }
    }

    return '\0';  // Can't seek backward or position not available
  }

  if (usingMemory_) {
    if (pos >= memorySize_) {
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
    if (pos >= bufferStartPos_ && pos < bufferStartPos_ + bufferLen_) {
      return (char)buffer_[pos - bufferStartPos_];
    }
    return '\0';
  }

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
  size_t savedFilePos = filePos_;

  for (size_t i = 0; i < len; i++) {
    char c = readChar();
    if (c != str[i]) {
      filePos_ = savedFilePos;
      return false;
    }
  }
  return true;
}

// ========== Forward Reading ==========

bool SimpleXmlParser::read() {
  if (!file_ && !usingMemory_ && !usingStream_) {
    currentNodeType_ = EndOfFile;
    return false;
  }

  // skip to the end of text node if we were in one
  if (currentNodeType_ == Text) {
    filePos_ = textNodeEndPos_;
    currentNodeType_ = None;
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
  elementStartPos_ = 0;
  elementEndPos_ = 0;

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
        return readEndElement();
      } else if (next == '!') {
        readChar();  // consume '!'
        char peek2 = peekChar();
        if (peek2 == '-') {
          return readComment();
        } else if (peek2 == '[') {
          return readCDATA();
        }
        // Skip unknown declaration
        skipToEndOfTag();
        continue;
      } else if (next == '?') {
        return readProcessingInstruction();
      } else {
        return readElement();
      }
    } else {
      return readText();
    }
  }

  return false;
}

bool SimpleXmlParser::readElement() {
  elementStartPos_ = filePos_ - 1;  // -1 because we already consumed '<'
  currentNodeType_ = Element;
  currentName_ = readElementName();
  parseAttributes();

  skipWhitespace();
  char c = peekChar();
  if (c == '/') {
    readChar();
    isEmptyElement_ = true;
  }

  while (true) {
    c = readChar();
    if (c == '>' || c == '\0')
      break;
  }
  elementEndPos_ = filePos_;

  return true;
}

bool SimpleXmlParser::readEndElement() {
  elementStartPos_ = filePos_ - 1;  // -1 because we already consumed '<'
  currentNodeType_ = EndElement;
  readChar();  // consume '/'
  currentName_ = readElementName();

  while (true) {
    char c = readChar();
    if (c == '>' || c == '\0')
      break;
  }
  elementEndPos_ = filePos_;

  return true;
}

bool SimpleXmlParser::readText() {
  elementStartPos_ = filePos_;
  currentNodeType_ = Text;
  textNodeStartPos_ = filePos_;
  textNodeCurrentPos_ = filePos_;

  // Clear streaming text buffer
  streamTextBuffer_ = "";
  streamTextBufferPos_ = 0;

  // For streaming mode, we can't scan backward - parent name will be empty
  // The caller should track element stack if needed
  currentName_ = "";

  if (!usingStream_) {
    // Find parent element name by scanning backward for the opening tag
    size_t searchPos = filePos_;
    while (searchPos > 0) {
      searchPos--;
      if (getByteAt(searchPos) == '>') {
        // Found end of previous tag, scan backward to find '<'
        size_t tagEnd = searchPos;
        while (searchPos > 0 && getByteAt(searchPos) != '<') {
          searchPos--;
        }
        if (getByteAt(searchPos) == '<') {
          // Check if this is an opening tag (not </ or <? or <!)
          char nextChar = getByteAt(searchPos + 1);
          if (nextChar != '/' && nextChar != '?' && nextChar != '!') {
            // Extract element name
            size_t nameStart = searchPos + 1;
            size_t nameEnd = nameStart;
            while (nameEnd < tagEnd) {
              char c = getByteAt(nameEnd);
              if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '>' || c == '/') {
                break;
              }
              nameEnd++;
            }
            for (size_t i = nameStart; i < nameEnd; i++) {
              currentName_ += (char)getByteAt(i);
            }
          }
        }
        break;
      }
    }
  }

  // Scan forward to find end of text and buffer content (for streaming mode)
  size_t scanPos = filePos_;
  bool hasNonWhitespace = false;

  while (true) {
    char c = getByteAt(scanPos);
    if (c == '\0' || c == '<') {
      break;
    }
    if (!hasNonWhitespace && c != ' ' && c != '\t' && c != '\n' && c != '\r') {
      hasNonWhitespace = true;
    }
    // In streaming mode, buffer the text as we scan
    if (usingStream_) {
      streamTextBuffer_ += c;
    }
    scanPos++;
  }

  textNodeEndPos_ = scanPos;
  elementEndPos_ = scanPos;

  // Skip whitespace-only text nodes
  if (!hasNonWhitespace) {
    filePos_ = textNodeEndPos_;
    return read();
  }

  return true;
}

bool SimpleXmlParser::readComment() {
  elementStartPos_ = filePos_ - 2;  // -2 for '<!' already consumed
  currentNodeType_ = Comment;
  currentValue_ = "";

  if (readChar() != '-' || peekChar() != '-') {
    skipToEndOfTag();
    elementEndPos_ = filePos_;
    return false;
  }
  readChar();  // consume second '-'

  while (true) {
    char c = readChar();
    if (c == '\0')
      break;

    if (c == '-' && peekChar() == '-') {
      readChar();
      if (peekChar() == '>') {
        readChar();
        break;
      }
      currentValue_ += '-';
      currentValue_ += '-';
    } else {
      currentValue_ += c;
    }
  }
  elementEndPos_ = filePos_;

  return true;
}

bool SimpleXmlParser::readCDATA() {
  elementStartPos_ = filePos_ - 2;  // -2 for '<!' already consumed
  currentNodeType_ = CDATA;
  currentValue_ = "";

  if (matchString("[CDATA[")) {
    while (true) {
      char c = readChar();
      if (c == '\0')
        break;

      if (c == ']' && peekChar() == ']') {
        readChar();
        if (peekChar() == '>') {
          readChar();
          break;
        }
        currentValue_ += ']';
        currentValue_ += ']';
      } else {
        currentValue_ += c;
      }
    }
  }
  elementEndPos_ = filePos_;

  return true;
}

bool SimpleXmlParser::readProcessingInstruction() {
  elementStartPos_ = filePos_ - 1;  // -1 for '<' already consumed
  currentNodeType_ = ProcessingInstruction;
  readChar();  // consume '?'

  currentName_ = readElementName();
  currentValue_ = "";

  while (true) {
    char c = readChar();
    if (c == '\0')
      break;

    if (c == '?' && peekChar() == '>') {
      readChar();
      break;
    }

    currentValue_ += c;
  }
  elementEndPos_ = filePos_;

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

    if (c == '>' || c == '/' || c == '\0')
      break;

    String attrName = readElementName();
    if (attrName.isEmpty())
      break;

    skipWhitespace();

    if (peekChar() != '=')
      break;
    readChar();

    skipWhitespace();

    char quote = peekChar();
    if (quote != '"' && quote != '\'')
      break;
    readChar();

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

    bool match = true;
    for (size_t j = 0; j < nameLen; j++) {
      char c1 = attrName.charAt(j);
      char c2 = name[j];
      if (c1 >= 'A' && c1 <= 'Z')
        c1 += 32;
      if (c2 >= 'A' && c2 <= 'Z')
        c2 += 32;
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

// ========== Text Node Character Reading ==========

char SimpleXmlParser::readTextNodeCharForward() {
  if (currentNodeType_ != Text) {
    return '\0';
  }

  hasPeekedTextNodeChar_ = false;

  // In streaming mode, read from buffered text
  if (usingStream_) {
    if (streamTextBufferPos_ >= (size_t)streamTextBuffer_.length()) {
      return '\0';
    }
    char c = streamTextBuffer_.charAt(streamTextBufferPos_);
    streamTextBufferPos_++;
    return c;
  }

  // Non-streaming mode: read from file/memory position
  if (textNodeEndPos_ > 0 && textNodeCurrentPos_ >= textNodeEndPos_) {
    return '\0';
  }

  char c = getByteAt(textNodeCurrentPos_);
  if (c == '\0' || c == '<') {
    return '\0';
  }

  textNodeCurrentPos_++;
  filePos_ = textNodeCurrentPos_;

  return c;
}

char SimpleXmlParser::peekTextNodeChar() {
  if (currentNodeType_ != Text) {
    return '\0';
  }

  if (hasPeekedTextNodeChar_) {
    return peekedTextNodeChar_;
  }

  // In streaming mode, peek from buffered text
  if (usingStream_) {
    if (streamTextBufferPos_ >= (size_t)streamTextBuffer_.length()) {
      return '\0';
    }
    peekedTextNodeChar_ = streamTextBuffer_.charAt(streamTextBufferPos_);
    hasPeekedTextNodeChar_ = true;
    return peekedTextNodeChar_;
  }

  // Non-streaming mode
  peekedTextNodeChar_ = getByteAt(textNodeCurrentPos_);

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

  // In streaming mode, check buffered text
  if (usingStream_) {
    return streamTextBufferPos_ < (size_t)streamTextBuffer_.length();
  }

  // Non-streaming mode
  if (textNodeEndPos_ > 0 && textNodeCurrentPos_ >= textNodeEndPos_) {
    return false;
  }

  char c = const_cast<SimpleXmlParser*>(this)->getByteAt(textNodeCurrentPos_);
  return c != '\0' && c != '<';
}