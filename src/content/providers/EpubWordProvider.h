#ifndef EPUB_WORD_PROVIDER_H
#define EPUB_WORD_PROVIDER_H

#include <SD.h>

#include <cstdint>
#include <vector>

#include "../css/CssParser.h"
#include "../css/CssStyle.h"
#include "../epub/EpubReader.h"
#include "../xml/SimpleXmlParser.h"
#include "StringWordProvider.h"
#include "WordProvider.h"

class EpubWordProvider : public WordProvider {
 public:
  // path: SD path to epub file or direct xhtml file
  // bufSize: decompressed text buffer size (default 4096)
  EpubWordProvider(const char* path, size_t bufSize = 4096);
  ~EpubWordProvider() override;
  bool isValid() const {
    return valid_;
  }

  bool hasNextWord() override;
  bool hasPrevWord() override;
  String getNextWord() override;
  String getPrevWord() override;

  float getPercentage() override;
  float getPercentage(int index) override;
  float getChapterPercentage() override;
  float getChapterPercentage(int index) override;
  void setPosition(int index) override;
  int getCurrentIndex() override;
  char peekChar(int offset = 0) override;
  int consumeChars(int n) override;
  bool isInsideWord() override;
  void ungetWord() override;
  void reset() override;

  // Chapter navigation
  int getChapterCount() override;
  int getCurrentChapter() override;
  bool setChapter(int chapterIndex) override;
  bool hasChapters() override {
    return isEpub_;
  }
  String getCurrentChapterName() override {
    return currentChapterName_;
  }

  // Style support
  CssStyle getCurrentStyle() override {
    return currentStyle_;
  }
  bool hasStyleSupport() override {
    return cssParser_ != nullptr;
  }

 private:
  // Opens a specific chapter (spine item) for reading
  bool openChapter(int chapterIndex);

  // Skip an element and all its children (for non-content elements like head, style, script)
  // elementName: the name of the element to skip
  // forward: true to skip forward (from start tag to end tag), false to skip backward (from end tag to start tag)
  // Returns true if successful, false if reached end/beginning of document
  bool skipElement(const String& elementName, bool forward);

  // Skip ahead to the next content (text or block element) or end of file
  // Used after returning a newline for block element end tags to ensure hasNextWord() returns false
  // when there's no more actual content
  void skipToNextContent();

  // Update the current style based on the element's class attribute
  void updateStyleForElement();

  bool valid_ = false;
  bool isEpub_ = false;  // True if source is EPUB, false if direct XHTML
  size_t bufSize_ = 0;

  String epubPath_;
  String xhtmlPath_;                  // Path to current extracted XHTML file
  String currentChapterName_;         // Cached chapter name from TOC
  EpubReader* epubReader_ = nullptr;  // Kept alive for chapter navigation
  SimpleXmlParser* parser_ = nullptr;
  int currentChapter_ = 0;  // Current chapter index (0-based)

  size_t prevFilePos_ = 0;      // Previous parser position for ungetWord()
  size_t fileSize_;             // Total file size for percentage calculation
  size_t firstContentPos_ = 0;  // Position of first readable content in chapter

  // CSS style tracking
  const CssParser* cssParser_ = nullptr;  // Borrowed from EpubReader, not owned
  CssStyle currentStyle_;                 // Currently active style (updated when entering elements)
};

#endif
