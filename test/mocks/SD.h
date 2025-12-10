#pragma once

#include <algorithm>
#include "platform_stubs.h"
#include "WString.h"
using std::min;
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

// File open modes
#define FILE_READ 0
#define FILE_WRITE 1

struct MockFile {
  std::string content;
  std::string filepath;
  size_t currentPos = 0;
  bool isOpen = false;
  bool isWriteMode = false;
  MockFile() {}
  ~MockFile() {
    close();
  }
  operator bool() const {
    return isOpen;
  }
  size_t size() const {
    return content.size();
  }
  size_t position() {
    return currentPos;
  }
  bool seek(size_t pos) {
    if (!isOpen)
      return false;
    currentPos = pos;
    return true;
  }
  size_t read(void* buf, size_t len) {
    if (!isOpen)
      return 0;
    size_t toRead = std::min(len, content.size() - currentPos);
    memcpy(buf, content.data() + currentPos, toRead);
    currentPos += toRead;
    return toRead;
  }
  int read() {
    if (!isOpen || currentPos >= content.size())
      return -1;
    return static_cast<unsigned char>(content[currentPos++]);
  }
  size_t write(const uint8_t* buf, size_t len) {
    if (!isOpen)
      return 0;
    content.append(reinterpret_cast<const char*>(buf), len);
    currentPos = content.size();
    return len;
  }
  size_t print(const char* str) {
    if (!isOpen || !str)
      return 0;
    size_t len = strlen(str);
    content.append(str, len);
    currentPos = content.size();
    return len;
  }
  size_t print(const String& str) {
    return print(str.c_str());
  }
  bool isDirectory() const { return false; }
  MockFile openNextFile() const { return MockFile(); }
  const char* name() const { return filepath.c_str(); }
  bool available() {
    return isOpen && currentPos < content.size();
  }
  void close() {
    if (isOpen && isWriteMode && !filepath.empty()) {
      // Write content to disk
      std::ofstream out(filepath, std::ios::binary);
      if (out.is_open()) {
        out.write(content.data(), content.size());
        out.close();
      }
    }
    isOpen = false;
    isWriteMode = false;
    content.clear();
    filepath.clear();
    currentPos = 0;
  }
};

struct MockSD {
  bool begin(int cs, MockSPI& spi, unsigned long freq) {
    (void)cs; (void)spi; (void)freq; return true;
  }
  MockFile open(const char* path, int mode = FILE_READ) {
    MockFile f;
    f.filepath = path;

    if (mode == FILE_WRITE) {
      // Write mode - create new file
      f.isOpen = true;
      f.isWriteMode = true;
    } else {
      // Read mode - load existing file
      std::ifstream in(path, std::ios::binary);
      if (in.is_open()) {
        f.isOpen = true;
        std::string& content = f.content;
        in.seekg(0, std::ios::end);
        content.resize(in.tellg());
        in.seekg(0, std::ios::beg);
        in.read(content.data(), content.size());
        in.close();
      }
    }
    return f;
  }
  bool exists(const char* path) {
    std::ifstream in(path);
    return in.good();
  }
  bool mkdir(const char* path) {
#ifdef _WIN32
    return _mkdir(path) == 0 || errno == EEXIST;
#else
    return ::mkdir(path, 0755) == 0 || errno == EEXIST;
#endif
  }
  bool remove(const char* path) {
    return std::remove(path) == 0;
  }
};

extern MockSD SD;
typedef MockFile File;