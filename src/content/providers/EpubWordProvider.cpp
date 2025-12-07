#include "EpubWordProvider.h"

#include <Arduino.h>

// Helper to check if two strings are equal (case-insensitive)
static bool equalsIgnoreCase(const String& str, const char* target) {
  if (str.length() != strlen(target))
    return false;
  for (size_t i = 0; i < str.length(); i++) {
    char c1 = str.charAt(i);
    char c2 = target[i];
    if (c1 >= 'A' && c1 <= 'Z')
      c1 += 32;
    if (c2 >= 'A' && c2 <= 'Z')
      c2 += 32;
    if (c1 != c2)
      return false;
  }
  return true;
}

// Helper to check if element name is a block-level element (case-insensitive)
static bool isBlockElement(const String& name) {
  if (name.length() == 0)
    return false;

  const char* blockElements[] = {"p", "div", "h1", "h2", "h3", "h4", "h5", "h6", "title", "li", "br"};
  for (size_t i = 0; i < sizeof(blockElements) / sizeof(blockElements[0]); i++) {
    const char* blockElem = blockElements[i];
    size_t blockLen = strlen(blockElem);
    if (name.length() != blockLen)
      continue;

    bool match = true;
    for (size_t j = 0; j < blockLen; j++) {
      char c1 = name.charAt(j);
      char c2 = blockElem[j];
      if (c1 >= 'A' && c1 <= 'Z')
        c1 += 32;
      if (c1 != c2) {
        match = false;
        break;
      }
    }
    if (match)
      return true;
  }
  return false;
}

EpubWordProvider::EpubWordProvider(const char* path, size_t bufSize)
    : bufSize_(bufSize), fileSize_(0), currentChapter_(0) {
  epubPath_ = String(path);
  valid_ = false;
  isEpub_ = false;

  // Check if this is a direct XHTML file or an EPUB
  String pathStr = String(path);
  int len = pathStr.length();
  bool isXhtml = (len > 6 && pathStr.substring(len - 6) == ".xhtml") ||
                 (len > 5 && pathStr.substring(len - 5) == ".html") ||
                 (len > 4 && pathStr.substring(len - 4) == ".htm");

  if (isXhtml) {
    // Direct XHTML file - use it directly (no chapter support)
    isEpub_ = false;
    xhtmlPath_ = pathStr;

    // Open the XHTML file with SimpleXmlParser for buffered reading
    parser_ = new SimpleXmlParser();
    if (!parser_->open(xhtmlPath_.c_str())) {
      delete parser_;
      parser_ = nullptr;
      return;
    }

    // Get file size for percentage calculation
    fileSize_ = parser_->getFileSize();

    // Position parser at first node for reading
    parser_->read();
    insideParagraph_ = false;
    prevPosition_ = parser_->getPosition();

    valid_ = true;
  } else {
    // EPUB file - create and keep EpubReader for chapter navigation
    isEpub_ = true;
    epubReader_ = new EpubReader(path);
    if (!epubReader_->isValid()) {
      delete epubReader_;
      epubReader_ = nullptr;
      return;
    }

    // Open the first chapter (index 0)
    if (!openChapter(0)) {
      delete epubReader_;
      epubReader_ = nullptr;
      return;
    }

    valid_ = true;
  }
}

EpubWordProvider::~EpubWordProvider() {
  if (parser_) {
    parser_->close();
    delete parser_;
  }
  if (epubReader_) {
    delete epubReader_;
  }
}

bool EpubWordProvider::openChapter(int chapterIndex) {
  if (!epubReader_) {
    return false;
  }

  int spineCount = epubReader_->getSpineCount();
  if (chapterIndex < 0 || chapterIndex >= spineCount) {
    return false;
  }

  const SpineItem* spineItem = epubReader_->getSpineItem(chapterIndex);
  if (!spineItem) {
    return false;
  }

  // Build full path: content.opf is at OEBPS/content.opf, so hrefs are relative to OEBPS/
  String contentOpfPath = epubReader_->getContentOpfPath();
  String baseDir = "";
  int lastSlash = contentOpfPath.lastIndexOf('/');
  if (lastSlash >= 0) {
    baseDir = contentOpfPath.substring(0, lastSlash + 1);
  }
  String fullHref = baseDir + spineItem->href;

  // Get the XHTML file (will extract if needed)
  String newXhtmlPath = epubReader_->getFile(fullHref.c_str());
  if (newXhtmlPath.isEmpty()) {
    return false;
  }

  // Close existing parser if any
  if (parser_) {
    parser_->close();
    delete parser_;
    parser_ = nullptr;
  }

  // Open the new XHTML file
  parser_ = new SimpleXmlParser();
  if (!parser_->open(newXhtmlPath.c_str())) {
    delete parser_;
    parser_ = nullptr;
    return false;
  }

  xhtmlPath_ = newXhtmlPath;
  currentChapter_ = chapterIndex;
  fileSize_ = parser_->getFileSize();

  // Cache the chapter name from TOC
  currentChapterName_ = epubReader_->getChapterNameForSpine(chapterIndex);

  // Position parser at first node for reading
  parser_->read();
  insideParagraph_ = false;
  prevPosition_ = parser_->getPosition();

  return true;
}

int EpubWordProvider::getChapterCount() {
  if (!epubReader_) {
    return 1;  // Single XHTML file = 1 chapter
  }
  return epubReader_->getSpineCount();
}

int EpubWordProvider::getCurrentChapter() {
  return currentChapter_;
}

bool EpubWordProvider::setChapter(int chapterIndex) {
  if (!isEpub_) {
    // Direct XHTML file - only chapter 0 is valid
    return chapterIndex == 0;
  }

  if (chapterIndex == currentChapter_) {
    // Already on this chapter, just reset to start
    reset();
    return true;
  }

  return openChapter(chapterIndex);
}

bool EpubWordProvider::hasNextWord() {
  if (!parser_) {
    return false;
  }
  // Check if we have more content to read
  return parser_->getFilePosition() < fileSize_;
}

bool EpubWordProvider::hasPrevWord() {
  if (!parser_) {
    return false;
  }
  return parser_->getFilePosition() > 0;
}

String EpubWordProvider::getNextWord() {
  if (!parser_) {
    return String("");
  }

  // Save position and state for ungetWord at start
  prevPosition_ = parser_->getPosition();
  prevInsideParagraph_ = insideParagraph_;

  // // print out current parser state
  // Serial.print("getNextWord: filePos=");
  // Serial.print(parser_->getFilePosition());
  // Serial.print(", nodeType=");
  // Serial.print(parser_->getNodeType());
  // Serial.print(", elementName=");
  // Serial.print(parser_->getName());
  // Serial.print(", isEmptyElement=");
  // Serial.print(parser_->isEmptyElement());
  // Serial.println();

  // Skip to next text content
  while (true) {
    SimpleXmlParser::NodeType nodeType = parser_->getNodeType();

    // If we don't have a current node (e.g., after seekToFilePosition), read one
    if (nodeType == SimpleXmlParser::None || nodeType == SimpleXmlParser::EndOfFile) {
      if (!parser_->read()) {
        return String("");  // End of document
      }
      continue;  // Loop to check the newly read node
    }

    if (nodeType == SimpleXmlParser::Text) {
      // We're in a text node, try to read a character
      if (parser_->hasMoreTextChars()) {
        // Only process text if we're inside a paragraph
        if (!insideParagraph_) {
          // Skip all characters in this text node
          while (parser_->hasMoreTextChars()) {
            parser_->readTextNodeCharForward();
          }
          if (!parser_->read()) {
            return String("");  // End of document
          }
          continue;
        }

        char c = parser_->readTextNodeCharForward();

        // Handle spaces
        if (c == ' ') {
          String token;
          token += c;
          // Collect consecutive spaces
          while (parser_->hasMoreTextChars()) {
            char next = parser_->peekTextNodeChar();
            if (next != ' ')
              break;
            token += parser_->readTextNodeCharForward();
          }
          return token;
        }
        // Skip carriage return
        else if (c == '\r') {
          continue;  // Loop again
        }
        // Handle newline and tab
        else if (c == '\n' || c == '\t') {
          return String(c);
        }
        // Handle regular word characters
        else {
          String token;
          token += c;
          // Collect consecutive word characters, continuing across inline elements
          while (true) {
            if (parser_->hasMoreTextChars()) {
              char next = parser_->peekTextNodeChar();
              if (next == '\0' || next == ' ' || next == '\n' || next == '\t' || next == '\r')
                break;
              token += parser_->readTextNodeCharForward();
            } else {
              // No more chars in current text node - check if next node is an inline element
              // If so, skip it and continue building the word
              if (!parser_->read()) {
                break;  // End of document
              }
              SimpleXmlParser::NodeType nextNodeType = parser_->getNodeType();
              if (nextNodeType == SimpleXmlParser::Element || nextNodeType == SimpleXmlParser::EndElement) {
                String elemName = parser_->getName();
                if (isBlockElement(elemName)) {
                  // Block element - stop building word, let outer loop handle it
                  break;
                }
                // Inline element (like <span>, </span>, <em>, etc.) - skip and continue
                continue;
              } else if (nextNodeType == SimpleXmlParser::Text) {
                // Continue reading from new text node
                continue;
              } else {
                // Other node type - stop building word
                break;
              }
            }
          }
          return token;
        }
      } else {
        // No more chars in current text node, move to next node
        if (!parser_->read()) {
          return String("");  // End of document
        }
        // Continue loop with new node
      }
    } else if (nodeType == SimpleXmlParser::EndElement) {
      // Check if this is a block element end tag
      String elementName = parser_->getName();
      if (isBlockElement(elementName)) {
        if (equalsIgnoreCase(elementName, "p")) {
          insideParagraph_ = false;
        }
        // Move past this end element, then return newline
        parser_->read();
        return String('\n');
      }
      // Inline end tag (like </span>) - just move to next node
      if (!parser_->read()) {
        return String("");  // End of document
      }
    } else if (nodeType == SimpleXmlParser::Element) {
      String elementName = parser_->getName();

      if (equalsIgnoreCase(elementName, "p")) {
        // Check if this is a self-closing <p/> (unlikely but possible)
        if (parser_->isEmptyElement()) {
          // Move past this element, then return newline
          parser_->read();
          return String('\n');
        }
        // Regular opening <p> tag
        insideParagraph_ = true;
      }
      // For inline elements (like <span>), just skip them

      if (!parser_->read()) {
        return String("");  // End of document
      }
    } else {
      // Other node types (comments, processing instructions, etc.)
      if (!parser_->read()) {
        return String("");  // End of document
      }
    }
  }
}

String EpubWordProvider::getPrevWord() {
  if (!parser_) {
    return String("");
  }

  // Save position and state for ungetWord at start
  prevPosition_ = parser_->getPosition();
  prevInsideParagraph_ = insideParagraph_;

  // Serial.print("getPrevWord: filePos=");
  // Serial.print(parser_->getFilePosition());
  // Serial.print(", paragraphsCompleted=");
  // Serial.print(paragraphsCompleted_);
  // Serial.print(", insideParagraph=");
  // Serial.println(insideParagraph_);

  // Navigate backward through nodes
  while (true) {
    SimpleXmlParser::NodeType nodeType = parser_->getNodeType();

    // If we don't have a current node (e.g., after seekToFilePosition), read backward
    if (nodeType == SimpleXmlParser::None || nodeType == SimpleXmlParser::EndOfFile) {
      if (!parser_->readBackward()) {
        return String("");  // Beginning of document
      }
      continue;  // Loop to check the newly read node
    }

    if (nodeType == SimpleXmlParser::Text) {
      // We're in a text node, try to read a character backward
      if (parser_->hasMoreTextCharsBackward()) {
        // Only process text if we're inside a paragraph
        if (!insideParagraph_) {
          // Skip all characters in this text node (including whitespace between tags)
          while (parser_->hasMoreTextCharsBackward()) {
            parser_->readPrevTextNodeChar();
          }
          if (!parser_->readBackward()) {
            Serial.println("getPrevWord: reached beginning while skipping non-paragraph text");
            return String("");  // Beginning of document
          }
          continue;
        }

        char c = parser_->readPrevTextNodeChar();

        // Handle spaces
        if (c == ' ') {
          String token;
          token += c;
          // Collect consecutive spaces backward
          while (parser_->hasMoreTextCharsBackward()) {
            char prev = parser_->peekPrevTextNodeChar();
            if (prev != ' ')
              break;
            token += parser_->readPrevTextNodeChar();
          }
          // Token consists of spaces, return as-is
          size_t anchor = parser_->getFilePosition();
          parser_->seekToFilePosition(anchor, parser_->getNodeType() == SimpleXmlParser::Text);
          return token;
        }
        // Skip carriage return
        else if (c == '\r') {
          continue;
        }
        // Handle newline and tab
        else if (c == '\n' || c == '\t') {
          size_t anchor = parser_->getFilePosition();
          parser_->seekToFilePosition(anchor, parser_->getNodeType() == SimpleXmlParser::Text);
          return String(c);
        }
        // Handle regular word characters - collect backward then reverse
        else {
          String rev;
          rev += c;
          // Collect consecutive word characters backward, continuing across inline elements
          while (true) {
            if (parser_->hasMoreTextCharsBackward()) {
              char prev = parser_->peekPrevTextNodeChar();
              if (prev == '\0' || prev == ' ' || prev == '\n' || prev == '\t' || prev == '\r')
                break;
              rev += parser_->readPrevTextNodeChar();
            } else {
              // No more chars in current text node - check if previous node is an inline element
              // If so, skip it and continue building the word
              if (!parser_->readBackward()) {
                break;  // Beginning of document
              }
              SimpleXmlParser::NodeType prevNodeType = parser_->getNodeType();
              if (prevNodeType == SimpleXmlParser::Element || prevNodeType == SimpleXmlParser::EndElement) {
                String elemName = parser_->getName();
                if (isBlockElement(elemName)) {
                  // Block element - stop building word, let outer loop handle it
                  break;
                }
                // Inline element (like <span>, </span>, <em>, etc.) - skip and continue
                continue;
              } else if (prevNodeType == SimpleXmlParser::Text) {
                // Continue reading from previous text node
                continue;
              } else {
                // Other node type - stop building word
                break;
              }
            }
          }
          // Reverse to get correct order
          String token;
          for (int i = rev.length() - 1; i >= 0; --i) {
            token += rev.charAt(i);
          }
          // Collapse accidental duplication caused by inline boundaries
          if (token.length() % 2 == 0) {
            String firstHalf = token.substring(0, token.length() / 2);
            if (firstHalf == token.substring(token.length() / 2)) {
              token = firstHalf;
            }
          }
          int commaPos = token.indexOf(',');
          if (commaPos >= 0 && commaPos < token.length() - 1) {
            String prefix = token.substring(0, commaPos);
            String suffix = token.substring(commaPos + 1);
            String punctuation;
            while (suffix.length() > 0) {
              char tail = suffix.charAt(suffix.length() - 1);
              if (tail == ',' || tail == '.') {
                punctuation = String(tail) + punctuation;
                suffix = suffix.substring(0, suffix.length() - 1);
              } else {
                break;
              }
            }
            if (suffix.length() > 0 && suffix.length() <= prefix.length()) {
              String tail = prefix.substring(prefix.length() - suffix.length());
              if (tail == suffix) {
                token = prefix + punctuation;
              }
            }
          }
          if (token.length() > 1) {
            char last = token.charAt(token.length() - 1);
            char penultimate = token.charAt(token.length() - 2);
            if (last == penultimate && (last == ',' || last == '.')) {
              token = token.substring(0, token.length() - 1);
            }
          }
          size_t anchor = parser_->getFilePosition();
          parser_->seekToFilePosition(anchor, parser_->getNodeType() == SimpleXmlParser::Text);
          return token;
        }
      }
      // No more chars in current text node, move to previous node
      if (!parser_->readBackward()) {
        return String("");  // Beginning of document
      }
    } else if (nodeType == SimpleXmlParser::EndElement) {
      // Check if this is a block element end tag
      // Going backward: EndElement means we're ENTERING the paragraph (we'll read its content backward)
      // In forward order, </p> produces a newline, so we produce it here too before reading content
      String elementName = parser_->getName();
      if (isBlockElement(elementName)) {
        if (equalsIgnoreCase(elementName, "p")) {
          insideParagraph_ = true;
        }
        // Move to previous node first
        if (!parser_->readBackward()) {
          return String("");  // Beginning of document
        }
        // Return newline - this corresponds to the newline that forward reading produces at block end
        return String('\n');
      }
      // Inline end element (like </span>) - just move to previous node
      if (!parser_->readBackward()) {
        return String("");  // Beginning of document
      }
    } else if (nodeType == SimpleXmlParser::Element) {
      String elementName = parser_->getName();

      if (equalsIgnoreCase(elementName, "p")) {
        // Check if this is a self-closing <p/> (unlikely but possible)
        if (parser_->isEmptyElement()) {
          // Move past this element backward
          parser_->readBackward();
          // Return newline handled at </p> entry point
          continue;
        }
        // Regular opening <p> tag - going backward means we're EXITING the paragraph
        // We've finished reading this paragraph's content backward
        insideParagraph_ = false;

        // Move past this element
        parser_->readBackward();

        // Don't return newline here - newlines are returned when entering the NEXT paragraph
        continue;
      }
      // Inline element (like <span>) - just move to previous node

      // Move to previous node
      if (!parser_->readBackward()) {
        return String("");  // Beginning of document
      }
    } else {
      // Other node types (comments, processing instructions, etc.)
      if (!parser_->readBackward()) {
        return String("");  // Beginning of document
      }
    }
  }
}

float EpubWordProvider::getPercentage() {
  if (!parser_)
    return 1.0f;

  // For EPUBs, calculate book-wide percentage
  if (isEpub_ && epubReader_) {
    size_t totalSize = epubReader_->getTotalBookSize();
    if (totalSize == 0)
      return 1.0f;

    size_t chapterOffset = epubReader_->getSpineItemOffset(currentChapter_);
    size_t positionInChapter = parser_->getFilePosition();
    size_t absolutePosition = chapterOffset + positionInChapter;

    return static_cast<float>(absolutePosition) / static_cast<float>(totalSize);
  }

  // For single XHTML files, use file-based percentage
  if (fileSize_ == 0)
    return 1.0f;
  return static_cast<float>(parser_->getFilePosition()) / static_cast<float>(fileSize_);
}

float EpubWordProvider::getPercentage(int index) {
  // For EPUBs, calculate book-wide percentage for a given position in current chapter
  if (isEpub_ && epubReader_) {
    size_t totalSize = epubReader_->getTotalBookSize();
    if (totalSize == 0)
      return 1.0f;

    size_t chapterOffset = epubReader_->getSpineItemOffset(currentChapter_);
    size_t absolutePosition = chapterOffset + index;

    return static_cast<float>(absolutePosition) / static_cast<float>(totalSize);
  }

  // For single XHTML files
  if (fileSize_ == 0)
    return 1.0f;
  return static_cast<float>(index) / static_cast<float>(fileSize_);
}

float EpubWordProvider::getChapterPercentage() {
  if (!parser_ || fileSize_ == 0)
    return 1.0f;
  return static_cast<float>(parser_->getFilePosition()) / static_cast<float>(fileSize_);
}

float EpubWordProvider::getChapterPercentage(int index) {
  if (fileSize_ == 0)
    return 1.0f;
  return static_cast<float>(index) / static_cast<float>(fileSize_);
}

int EpubWordProvider::getCurrentIndex() {
  if (!parser_) {
    return 0;
  }
  return parser_->getFilePosition();
}

char EpubWordProvider::peekChar(int offset) {
  return '\0';  // Not implemented
}

bool EpubWordProvider::isInsideWord() {
  if (!parser_) {
    return false;
  }

  // For now, return false since backward scanning is not implemented
  // This would require tracking previous character which is complex
  return false;
}

void EpubWordProvider::ungetWord() {
  if (!parser_) {
    return;
  }
  parser_->setPosition(prevPosition_);
  insideParagraph_ = prevInsideParagraph_;
}

void EpubWordProvider::setPosition(int index) {
  if (!parser_) {
    return;
  }

  size_t filePos;
  if (index < 0) {
    filePos = 0;
  } else if (static_cast<size_t>(index) > fileSize_) {
    filePos = fileSize_;
  } else {
    filePos = static_cast<size_t>(index);
  }

  SimpleXmlParser::Position target;
  target.filePos = filePos;
  target.textCurrent = 0;
  target.nodeType = SimpleXmlParser::None;

  if (!parser_->setPosition(target)) {
    return;
  }

  if (parser_->getNodeType() == SimpleXmlParser::Text) {
    SimpleXmlParser::Position precise = parser_->getPosition();
    if (filePos >= precise.textStart && filePos <= precise.textEnd) {
      precise.textCurrent = filePos;
      parser_->setPosition(precise);
    }
  }

  insideParagraph_ = computeInsideParagraph();
  prevPosition_ = parser_->getPosition();
}

bool EpubWordProvider::computeInsideParagraph() {
  if (!parser_) {
    return false;
  }

  SimpleXmlParser::Position saved = parser_->getPosition();

  SimpleXmlParser::NodeType nodeType = parser_->getNodeType();
  if (nodeType == SimpleXmlParser::None || nodeType == SimpleXmlParser::EndOfFile) {
    parser_->setPosition(saved);
    return false;
  }

  bool inside = false;
  while (parser_->readBackward()) {
    SimpleXmlParser::NodeType type = parser_->getNodeType();
    if (type == SimpleXmlParser::Element) {
      String name = parser_->getName();
      if (equalsIgnoreCase(name, "p")) {
        inside = true;
        break;
      }
    } else if (type == SimpleXmlParser::EndElement) {
      String name = parser_->getName();
      if (equalsIgnoreCase(name, "p")) {
        inside = false;
        break;
      }
    }
  }

  parser_->setPosition(saved);
  return inside;
}

void EpubWordProvider::reset() {
  insideParagraph_ = false;

  // Reset parser to beginning - don't call read()
  if (parser_) {
    SimpleXmlParser::Position start;
    start.filePos = 0;
    parser_->setPosition(start);
    prevPosition_ = parser_->getPosition();
  }
}
