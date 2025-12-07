#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include "content/xml/SimpleXmlParser.h"
#include "test_config.h"
#include "test_utils.h"

struct NodeSnapshot {
  SimpleXmlParser::NodeType type;
  String name;
  bool isEmpty = false;
  String text;
  SimpleXmlParser::Position position;
};

String readTextForward(SimpleXmlParser& parser) {
  String text = "";
  while (parser.hasMoreTextChars()) {
    text += parser.readTextNodeCharForward();
  }
  return text;
}

String readTextBackward(SimpleXmlParser& parser) {
  String text = "";
  while (parser.hasMoreTextCharsBackward()) {
    text = String(parser.readPrevTextNodeChar()) + text;
  }
  return text;
}

std::vector<NodeSnapshot> readForwardNodes(TestUtils::TestRunner& runner, const char* path) {
  SimpleXmlParser parser;
  std::vector<NodeSnapshot> result;

  runner.expectTrue(parser.open(path), "Open XHTML for forward pass");
  while (parser.read()) {
    NodeSnapshot snap;
    snap.type = parser.getNodeType();
    snap.name = parser.getName();
    snap.isEmpty = parser.isEmptyElement();
    snap.position = parser.getPosition();

    if (snap.type == SimpleXmlParser::Text) {
      snap.text = readTextForward(parser);
    }

    result.push_back(snap);
  }
  parser.close();
  return result;
}

std::vector<NodeSnapshot> readBackwardNodes(TestUtils::TestRunner& runner, const char* path) {
  SimpleXmlParser parser;
  std::vector<NodeSnapshot> result;

  runner.expectTrue(parser.open(path), "Open XHTML for backward pass");
  SimpleXmlParser::Position endPos;
  endPos.filePos = parser.getFileSize();
  parser.setPosition(endPos);

  while (parser.readBackward()) {
    NodeSnapshot snap;
    snap.type = parser.getNodeType();
    snap.name = parser.getName();
    snap.isEmpty = parser.isEmptyElement();
    snap.position = parser.getPosition();

    if (snap.type == SimpleXmlParser::Text) {
      snap.text = readTextBackward(parser);
    }

    if (!result.empty()) {
      const auto& last = result.back();
      if (last.type == snap.type && last.name == snap.name && last.isEmpty == snap.isEmpty &&
          last.position.filePos == snap.position.filePos) {
        continue;
      }
    }

    result.push_back(snap);
  }
  parser.close();
  return result;
}

void testForwardBackwardSymmetry(TestUtils::TestRunner& runner, const char* path) {
  auto forward = readForwardNodes(runner, path);
  auto backward = readBackwardNodes(runner, path);

  runner.expectTrue(!forward.empty(), "Forward pass captured nodes");
  runner.expectTrue(!backward.empty(), "Backward pass captured nodes");

  size_t compareCount = std::min(forward.size(), backward.size());
  bool sizeOk = std::max(forward.size(), backward.size()) - compareCount <= 1;
  runner.expectTrue(sizeOk, "Comparable node counts (tolerance 1)");
  std::cout << "Forward nodes: " << forward.size() << ", backward nodes: " << backward.size() << "\n";

  if (compareCount > 0) {
    const auto& fFirst = forward.front();
    const auto& bLast = backward.back();
    runner.expectTrue(fFirst.type == bLast.type, "First forward node mirrors last backward node type");
  }
}

void testPositionRestoration(TestUtils::TestRunner& runner, const char* path,
                             const std::vector<NodeSnapshot>& snapshots) {
  SimpleXmlParser parser;
  runner.expectTrue(parser.open(path), "Open XHTML for restoration");

  for (size_t i = 0; i < snapshots.size(); i++) {
    const auto& snap = snapshots[i];
    // Only restore by file position (and text offset), ensuring no cached metadata is needed.
    SimpleXmlParser::Position seekPos;
    seekPos.filePos = snap.position.filePos;
    seekPos.textCurrent = snap.position.textCurrent;

    bool setOk = parser.setPosition(seekPos);
    runner.expectTrue(setOk, "Set position for node " + std::to_string(i));
    runner.expectTrue(parser.getNodeType() == snap.type, "Node type restored for node " + std::to_string(i));
    runner.expectTrue(parser.getName() == snap.name, "Node name restored for node " + std::to_string(i));
    runner.expectTrue(parser.isEmptyElement() == snap.isEmpty, "Empty flag restored for node " + std::to_string(i));

    if (snap.type == SimpleXmlParser::Text) {
      String rebuilt = readTextForward(parser);
      runner.expectTrue(rebuilt == snap.text, "Text restored for node " + std::to_string(i));
    }
  }

  parser.close();
}

void testMidTextNavigation(TestUtils::TestRunner& runner, const char* path) {
  SimpleXmlParser parser;
  runner.expectTrue(parser.open(path), "Open XHTML for mid-text test");

  while (parser.read()) {
    if (parser.getNodeType() != SimpleXmlParser::Text)
      continue;

    SimpleXmlParser::Position start = parser.getPosition();
    String fullText = readTextForward(parser);
    size_t offset = std::min<size_t>(fullText.length() / 2, 30);
    if (fullText.length() == 0)
      break;

    bool setStart = parser.setPosition(start);
    runner.expectTrue(setStart, "Reset to start of text node");

    for (size_t i = 0; i < offset && parser.hasMoreTextChars(); i++) {
      parser.readTextNodeCharForward();
    }

    SimpleXmlParser::Position mid = parser.getPosition();
    String fromMid = readTextForward(parser);
    runner.expectTrue(fromMid == fullText.substring(offset), "Resumed text matches from offset");

    bool setMid = parser.setPosition(mid);
    runner.expectTrue(setMid, "Return to mid text position");

    String backToStart = readTextBackward(parser);
    runner.expectTrue(backToStart == fullText.substring(0, offset), "Backward navigation restores prefix");
    break;
  }

  parser.close();
}

int main() {
  TestUtils::TestRunner runner("SimpleXmlParser Position Test");
  const char* xhtmlPath = "data/books/1A9A8A09379E4577B2346DECBE09D19A.xhtml";

  auto forward = readForwardNodes(runner, xhtmlPath);
  testForwardBackwardSymmetry(runner, xhtmlPath);
  testPositionRestoration(runner, xhtmlPath, forward);
  testMidTextNavigation(runner, xhtmlPath);

  return runner.allPassed() ? 0 : 1;
}
