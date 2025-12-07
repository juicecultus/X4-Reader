#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include "content/xml/SimpleXmlParser.h"
#include "test_utils.h"

namespace {

const char* kFixturePath = "data/books/1A9A8A09379E4577B2346DECBE09D19A.xhtml";

struct TextSpan {
  SimpleXmlParser::Position pos;
  size_t start;
  size_t end;
};

std::vector<SimpleXmlParser::Position> collectPositions(SimpleXmlParser& parser, std::vector<TextSpan>& textSpans) {
  std::vector<SimpleXmlParser::Position> positions;

  while (parser.read()) {
    auto pos = parser.getPosition();
    positions.push_back(pos);

    if (pos.nodeType == SimpleXmlParser::Text && pos.textEnd > pos.textStart) {
      textSpans.push_back({pos, pos.textStart, pos.textEnd});
    }
  }

  return positions;
}

std::vector<SimpleXmlParser::Position> collectBackwardPositions(SimpleXmlParser& parser) {
  std::vector<SimpleXmlParser::Position> positions;

  // Start from end of file
  parser.seekToFilePosition(parser.getFileSize());

  while (parser.readBackward()) {
    positions.push_back(parser.getPosition());
  }

  return positions;
}

bool comparePositions(const SimpleXmlParser::Position& a, const SimpleXmlParser::Position& b, std::string& message) {
  if (a.nodeType != b.nodeType) {
    message = "node type mismatch";
    return false;
  }
  if (a.name != b.name) {
    message = "name mismatch";
    return false;
  }
  if (a.isEmpty != b.isEmpty) {
    message = "empty flag mismatch";
    return false;
  }
  if (a.textStart != b.textStart || a.textEnd != b.textEnd || a.textCurrent != b.textCurrent) {
    message = "text bounds mismatch";
    return false;
  }
  if (a.attributes.size() != b.attributes.size()) {
    message = "attribute count mismatch";
    return false;
  }
  for (size_t i = 0; i < a.attributes.size(); ++i) {
    if (a.attributes[i].first != b.attributes[i].first || a.attributes[i].second != b.attributes[i].second) {
      message = "attribute mismatch";
      return false;
    }
  }
  return true;
}

void testPositionRestoration(TestUtils::TestRunner& runner) {
  SimpleXmlParser parser;
  runner.expectTrue(parser.open(kFixturePath), "open fixture");
  std::vector<TextSpan> textSpans;
  auto positions = collectPositions(parser, textSpans);

  // Also collect backward positions to ensure setPosition works in both directions
  auto backwardPositions = collectBackwardPositions(parser);

  runner.expectTrue(!positions.empty(), "positions collected");

  // Sample a subset of positions to keep runtime manageable
  std::vector<size_t> sampleIndices;
  const size_t stride = positions.size() < 50 ? 1 : positions.size() / 25;
  for (size_t i = 0; i < positions.size(); i += stride) {
    sampleIndices.push_back(i);
  }
  if (sampleIndices.back() != positions.size() - 1) {
    sampleIndices.push_back(positions.size() - 1);
  }

  for (size_t idx : sampleIndices) {
    SimpleXmlParser restore;
    runner.expectTrue(restore.open(kFixturePath), "reopen fixture");
    const auto& expected = positions[idx];
    bool ok = restore.setPosition(expected);
    if (!runner.expectTrue(ok, "setPosition works")) {
      continue;
    }
    auto actual = restore.getPosition();
    std::string reason;
    if (!comparePositions(expected, actual, reason)) {
      std::cout << "Forward mismatch at sample " << idx << " filePos=" << expected.filePos
                << " expectedType=" << expected.nodeType << " actualType=" << actual.nodeType
                << " expectedName=" << expected.name.c_str() << " actualName=" << actual.name.c_str()
                << "\n";
    }
    runner.expectTrue(comparePositions(expected, actual, reason), "position round trip", reason);

    if (actual.nodeType == SimpleXmlParser::Text) {
      char expectedChar = '\0';
      if (expected.textCurrent < expected.textEnd) {
        expectedChar = TestUtils::readFile(kFixturePath).at(expected.textCurrent);
      }
      char peeked = restore.peekTextNodeChar();
      if (expectedChar == '\0') {
        runner.expectTrue(peeked == '\0', "peek at end of text");
      } else {
        runner.expectTrue(peeked == expectedChar, "peek preserves offset");
      }
    }
  }

  // Repeat the sampling for backward-collected positions
  if (!backwardPositions.empty()) {
    std::vector<size_t> backSamples;
    const size_t backStride = backwardPositions.size() < 50 ? 1 : backwardPositions.size() / 25;
    for (size_t i = 0; i < backwardPositions.size(); i += backStride) {
      backSamples.push_back(i);
    }
    if (backSamples.back() != backwardPositions.size() - 1) {
      backSamples.push_back(backwardPositions.size() - 1);
    }

    for (size_t idx : backSamples) {
      SimpleXmlParser restore;
      runner.expectTrue(restore.open(kFixturePath), "reopen fixture (backward)");
      const auto& expected = backwardPositions[idx];
      bool ok = restore.setPosition(expected);
      if (!runner.expectTrue(ok, "setPosition works (backward)")) {
        continue;
      }
      auto actual = restore.getPosition();
      std::string reason;
      if (!comparePositions(expected, actual, reason)) {
        std::cout << "Backward mismatch at sample " << idx << " filePos=" << expected.filePos
                  << " expectedType=" << expected.nodeType << " actualType=" << actual.nodeType << "\n";
        reason += " at index " + std::to_string(idx) + " (filePos=" + std::to_string(expected.filePos) + ")";
      }
      runner.expectTrue(comparePositions(expected, actual, reason), "position round trip (backward)", reason);
    }
  }
}

void testTextCursorRestoration(TestUtils::TestRunner& runner) {
  SimpleXmlParser parser;
  runner.expectTrue(parser.open(kFixturePath), "open fixture for text test");
  std::vector<TextSpan> textSpans;
  auto positions = collectPositions(parser, textSpans);
  (void)positions;

  auto fileContent = TestUtils::readFile(kFixturePath);
  runner.expectTrue(!fileContent.empty(), "fixture loaded for text test");

  const size_t limit = std::min<size_t>(textSpans.size(), 8);
  for (size_t i = 0; i < limit; ++i) {
    const auto& span = textSpans[i];
    const size_t length = span.end - span.start;
    std::vector<size_t> offsets = {span.start};
    if (length > 2) {
      offsets.push_back(span.start + length / 2);
      offsets.push_back(span.end - 1);
    }

    for (size_t offset : offsets) {
      SimpleXmlParser cursor;
      runner.expectTrue(cursor.open(kFixturePath), "reopen for text cursor");
      auto pos = span.pos;
      pos.textCurrent = offset;
      pos.filePos = offset;

      bool ok = cursor.setPosition(pos);
      if (!runner.expectTrue(ok, "setPosition inside text")) {
        continue;
      }

      std::string expected = fileContent.substr(offset, span.end - offset);
      std::string actual;
      char c;
      while ((c = cursor.readTextNodeCharForward()) != '\0') {
        actual.push_back(c);
      }
      runner.expectEqual(expected, actual, "forward text slice");

      // Restore to end to walk backward
      SimpleXmlParser backward;
      runner.expectTrue(backward.open(kFixturePath), "reopen for backward text");
      auto endPos = span.pos;
      endPos.textCurrent = span.end;
      endPos.filePos = span.end;
      ok = backward.setPosition(endPos);
      if (!runner.expectTrue(ok, "setPosition at text end")) {
        continue;
      }
      std::string backwardActual;
      while ((c = backward.readPrevTextNodeChar()) != '\0') {
        backwardActual.push_back(c);
      }
      std::string backwardExpected = fileContent.substr(span.start, span.end - span.start);
      std::reverse(backwardActual.begin(), backwardActual.end());
      runner.expectEqual(backwardExpected, backwardActual, "backward text slice");
    }
  }
}

}  // namespace

int main() {
  TestUtils::TestRunner runner("SimpleXmlParser Round Trip");
  testPositionRestoration(runner);
  testTextCursorRestoration(runner);
  return runner.allPassed() ? 0 : 1;
}

