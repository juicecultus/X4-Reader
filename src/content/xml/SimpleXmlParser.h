#ifndef SIMPLE_XML_PARSER_H
#define SIMPLE_XML_PARSER_H

#include <Arduino.h>
#include <SD.h>

#include <utility>
#include <vector>

/**
 * SimpleXmlParser - A buffered XML parser for reading attributes
 *
 * This parser uses a read buffer to minimize SD card I/O operations.
 * It's designed for simple tag and attribute extraction.
 */
class SimpleXmlParser {
 public:
  // Callback type for streaming input
  // Returns number of bytes read into buffer (0 for EOF, -1 for error)
  typedef int (*StreamCallback)(char* buffer, size_t maxSize, void* userData);

  SimpleXmlParser();
  ~SimpleXmlParser();

  /**
   * Open an XML file for parsing
   * Returns true if successful
   */
  bool open(const char* filepath);

  /**
   * Open XML from memory buffer for parsing
   * Returns true if successful
   */
  bool openFromMemory(const char* data, size_t dataSize);

  /**
   * Open XML from streaming callback for parsing
   * Returns true if successful
   */
  bool openFromStream(StreamCallback callback, void* userData);

  /**
   * Close the current file
   */
  void close();

  // ========== Node Navigation API ==========

  enum NodeType {
    None = 0,
    Element,                // Opening tag like <div>
    Text,                   // Text content
    EndElement,             // Closing tag like </div>
    Comment,                // <!-- comment -->
    ProcessingInstruction,  // <?xml ... ?>
    CDATA,                  // <![CDATA[ ... ]]>
    EndOfFile
  };

  /**
   * Read next node from XML stream
   * Returns true if a node was read, false if end of file
   * Call getNodeType() to determine what was read
   */
  bool read();

  /**
   * Get the type of the current node
   */
  NodeType getNodeType() const {
    return currentNodeType_;
  }

  /**
   * Get the name of the current element (for Element/EndElement nodes)
   * Returns empty string for other node types
   */
  String getName() const {
    return currentName_;
  }

  /**
   * Check if current element is empty (self-closing like <br/>)
   * Only valid for Element nodes
   */
  bool isEmptyElement() const {
    return isEmptyElement_;
  }

  /**
   * Get attribute value by name from current element
   * Returns empty string if attribute not found
   * Only valid for Element nodes
   */
  String getAttribute(const char* name) const;

  /**
   * Peek at next character in current text node without advancing
   * Only valid when on a Text node
   * Returns '\0' when end of text node reached
   */
  char peekTextNodeChar();

  /**
   * Check if there are more characters in current text node
   * Only valid when on a Text node
   */
  bool hasMoreTextChars() const;

  // Text node reading helpers
  char readTextNodeCharForward();

  /**
   * Get current file position (the cursor)
   */
  size_t getFilePosition() const {
    // For text nodes, return the current position within the text
    if (currentNodeType_ == Text) {
      return textNodeCurrentPos_;
    }
    return filePos_;
  }

  /**
   * Get the start position of the current element/node in the file
   * This is the position where the element begins (e.g., the '<' for tags)
   */
  size_t getElementStartPos() const {
    return elementStartPos_;
  }

  /**
   * Get the end position of the current element/node in the file
   * This is the position after the element ends (e.g., after '>' for tags)
   */
  size_t getElementEndPos() const {
    return elementEndPos_;
  }

  /**
   * Get total file size
   */
  size_t getFileSize() const {
    if (usingMemory_) {
      return memorySize_;
    }
    if (!file_) {
      return 0;
    }
    return file_.size();
  }

  size_t textNodeStartPos_;  // File position where text node content starts
  size_t textNodeEndPos_;    // File position where text node content ends

 private:
  File file_;
  const char* memoryData_;  // Pointer to memory buffer (if parsing from memory)
  size_t memorySize_;       // Size of memory buffer
  bool usingMemory_;        // True if parsing from memory instead of file

  // Streaming mode
  StreamCallback streamCallback_;  // Callback for streaming input
  void* streamUserData_;           // User data for callback
  bool usingStream_;               // True if parsing from stream
  size_t streamPosition_;          // Current position in stream (total bytes read)
  bool streamEOF_;                 // True when stream has reached EOF

  // Buffering for faster I/O
  static const size_t BUFFER_SIZE = 8192;
  static const size_t NUM_STREAM_BUFFERS = 3;  // Number of sliding window buffers for streaming

  uint8_t* buffer_;        // Primary buffer for file/memory mode (heap allocated to avoid stack overflow)
  size_t bufferStartPos_;  // File position of first byte in buffer
  size_t bufferLen_;       // Number of valid bytes in buffer
  size_t filePos_;         // Current position in file

  // Streaming sliding window buffers
  uint8_t* streamBuffers_[NUM_STREAM_BUFFERS];      // Array of buffer pointers
  size_t streamBufferStarts_[NUM_STREAM_BUFFERS];   // Start position of each buffer
  size_t streamBufferLengths_[NUM_STREAM_BUFFERS];  // Length of each buffer
  int streamCurrentBuffer_;                         // Index of most recently filled buffer

  // Helper functions
  char getByteAt(size_t pos);         // Get byte at any position, loading buffer if needed
  bool loadBufferAround(size_t pos);  // Load buffer centered around position
  bool skipWhitespace();
  bool matchString(const char* str);
  char readChar();
  char peekChar();

  // Node state
  struct Attribute {
    String name;
    String value;
  };

  NodeType currentNodeType_;
  String currentName_;
  String currentValue_;  // Only used for Comment, CDATA, ProcessingInstruction nodes
  bool isEmptyElement_;
  std::vector<Attribute> attributes_;

  // Text node reading state
  size_t textNodeCurrentPos_;   // Current position within text node
  char peekedTextNodeChar_;     // Cached character for peekTextNodeChar
  bool hasPeekedTextNodeChar_;  // Whether we have a peeked character

  // Text buffer for streaming mode (can't seek back, so buffer text when found)
  String streamTextBuffer_;     // Buffered text content for streaming mode
  size_t streamTextBufferPos_;  // Current read position in buffered text

  // Element/node position tracking
  size_t elementStartPos_;  // Start position of current element in file
  size_t elementEndPos_;    // End position of current element in file

  // XmlReader helper methods
  bool readElement();
  bool readEndElement();
  bool readText();
  bool readComment();
  bool readCDATA();
  bool readProcessingInstruction();
  void parseAttributes();
  String readElementName();
  void skipToEndOfTag();
};

#endif
