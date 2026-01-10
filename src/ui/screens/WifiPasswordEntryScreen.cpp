#include "WifiPasswordEntryScreen.h"

#include <resources/fonts/FontManager.h>
#include <resources/fonts/other/MenuFontSmall.h>
#include <resources/fonts/other/MenuHeader.h>

#include "../../core/Settings.h"
#include "../UIManager.h"

static const int kKeyboardRowCount = 5;
static const int kKeyboardCols = 10;

// Special "keys"; normal keys are just single printable chars.
static const char kKeyOk = '\x01';
static const char kKeyDel = '\x02';
static const char kKeySpace = '\x03';
static const char kKeyShift = '\x04';
static const char kKeySym = '\x05';

static const char kKeyboardAlpha[kKeyboardRowCount][kKeyboardCols] = {
    // Row0: digits
    {'1', '2', '3', '4', '5', '6', '7', '8', '9', '0'},
    // Row1
    {'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p'},
    // Row2
    {'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', kKeyDel},
    // Row3
    {kKeyShift, 'z', 'x', 'c', 'v', 'b', 'n', 'm', '-', '_'},
    // Row4
    {kKeyOk, kKeySpace, '.', '@', '/', '\\', ':', ';', kKeySym, '!'},
};

// Symbols layout: keep digits top row, make rest punctuation-heavy.
static const char kKeyboardSym[kKeyboardRowCount][kKeyboardCols] = {
    {'1', '2', '3', '4', '5', '6', '7', '8', '9', '0'},
    {'!', '@', '#', '$', '%', '^', '&', '*', '(', ')'},
    {'-', '_', '=', '+', '[', ']', '{', '}', '\\', kKeyDel},
    {kKeyShift, '<', '>', '?', '/', '|', '~', '`', '"', '\''},
    {kKeyOk, kKeySpace, '.', ',', ':', ';', kKeySym, '!', '&', '='},
};

static char getKeyAt(bool symbols, bool caps, int row, int col) {
  if (row < 0 || row >= kKeyboardRowCount || col < 0 || col >= kKeyboardCols)
    return 0;
  char k = symbols ? kKeyboardSym[row][col] : kKeyboardAlpha[row][col];
  if (!symbols && caps && k >= 'a' && k <= 'z') {
    k = (char)('A' + (k - 'a'));
  }
  return k;
}

static const char* getKeyLabel(char key, bool symbols, bool caps) {
  (void)symbols;
  (void)caps;
  switch (key) {
    case kKeyOk:
      return "OK";
    case kKeyDel:
      return "DL";
    case kKeySpace:
      return "SP";
    case kKeyShift:
      return "SH";
    case kKeySym:
      return "SY";
    default:
      return nullptr;
  }
}

WifiPasswordEntryScreen::WifiPasswordEntryScreen(EInkDisplay& display, TextRenderer& renderer, UIManager& uiManager)
    : display(display), textRenderer(renderer), uiManager(uiManager) {}

void WifiPasswordEntryScreen::begin() {
  loadSettings();
}

void WifiPasswordEntryScreen::activate() {
  loadSettings();
  editOriginal = wifiPass;
  editBuffer = wifiPass;
  keyRow = 1;
  keyCol = 0;
  caps = false;
  symbols = false;
}

void WifiPasswordEntryScreen::handleButtons(Buttons& buttons) {
  if (buttons.isPressed(Buttons::BACK)) {
    editBuffer = editOriginal;
    uiManager.showScreen(UIManager::ScreenId::WifiSettings);
  } else if (buttons.isPressed(Buttons::LEFT)) {
    keyCol++;
    if (keyCol >= kKeyboardCols)
      keyCol = 0;
    show();
  } else if (buttons.isPressed(Buttons::RIGHT)) {
    keyCol--;
    if (keyCol < 0)
      keyCol = kKeyboardCols - 1;
    show();
  } else if (buttons.isPressed(Buttons::VOLUME_UP)) {
    keyRow--;
    if (keyRow < 0)
      keyRow = kKeyboardRowCount - 1;
    show();
  } else if (buttons.isPressed(Buttons::VOLUME_DOWN)) {
    keyRow++;
    if (keyRow >= kKeyboardRowCount)
      keyRow = 0;
    show();
  } else if (buttons.isPressed(Buttons::CONFIRM)) {
    chooseKey();
  }
}

void WifiPasswordEntryScreen::show() {
  render();
  display.displayBuffer(EInkDisplay::FAST_REFRESH);
}

void WifiPasswordEntryScreen::loadSettings() {
  Settings& s = uiManager.getSettings();
  wifiPass = s.getString(String("wifi.pass"));
}

void WifiPasswordEntryScreen::saveSettings() {
  Settings& s = uiManager.getSettings();
  s.setString(String("wifi.pass"), wifiPass);
  if (!s.save()) {
    Serial.println("WifiPasswordEntryScreen: Failed to write settings.cfg");
  }
}

void WifiPasswordEntryScreen::chooseKey() {
  char key = getKeyAt(symbols, caps, keyRow, keyCol);
  if (key == 0)
    return;

  if (key == kKeyOk) {
    wifiPass = editBuffer;
    saveSettings();
    uiManager.showScreen(UIManager::ScreenId::WifiSettings);
    return;
  }

  if (key == kKeyDel) {
    if (editBuffer.length() > 0) {
      editBuffer.remove(editBuffer.length() - 1);
    }
    show();
    return;
  }

  if (key == kKeySpace) {
    if (editBuffer.length() < 64) {
      editBuffer += ' ';
    }
    show();
    return;
  }

  if (key == kKeyShift) {
    caps = !caps;
    show();
    return;
  }

  if (key == kKeySym) {
    symbols = !symbols;
    // When switching mode, keep cursor in bounds.
    if (keyRow < 0)
      keyRow = 0;
    if (keyRow >= kKeyboardRowCount)
      keyRow = kKeyboardRowCount - 1;
    if (keyCol < 0)
      keyCol = 0;
    if (keyCol >= kKeyboardCols)
      keyCol = kKeyboardCols - 1;
    show();
    return;
  }

  // Regular printable char
  if (key >= 32 && key <= 126 && editBuffer.length() < 64) {
    editBuffer += key;
  }
  show();
}

void WifiPasswordEntryScreen::render() {
  display.clearScreen(0xFF);
  textRenderer.setTextColor(TextRenderer::COLOR_BLACK);
  textRenderer.setFont(getTitleFont());

  textRenderer.setFrameBuffer(display.getFrameBuffer());
  textRenderer.setBitmapType(TextRenderer::BITMAP_BW);

  {
    const char* title = "WiFi Password";
    int16_t x1, y1;
    uint16_t w, h;
    textRenderer.getTextBounds(title, 0, 0, &x1, &y1, &w, &h);
    int16_t centerX = (480 - (int)w) / 2;
    textRenderer.setCursor(centerX, 75);
    textRenderer.print(title);
  }

  textRenderer.setFont(getMainFont());

  {
    String shown;
    for (int i = 0; i < editBuffer.length() && i < 32; ++i)
      shown += "*";
    if (editBuffer.length() > 32)
      shown += "...";

    String line = String("Password: ") + shown;
    int16_t x1, y1;
    uint16_t w, h;
    textRenderer.getTextBounds(line.c_str(), 0, 0, &x1, &y1, &w, &h);
    int16_t centerX = (480 - (int)w) / 2;
    textRenderer.setCursor(centerX, 200);
    textRenderer.print(line);
  }

  {
    // Keyboard grid
    const int startX = 24;
    const int startY = 260;
    const int cellW = 44;
    const int cellH = 30;

    for (int r = 0; r < kKeyboardRowCount; ++r) {
      for (int c = 0; c < kKeyboardCols; ++c) {
        char key = getKeyAt(symbols, caps, r, c);
        if (key == 0)
          continue;

        const char* special = getKeyLabel(key, symbols, caps);
        String label;
        if (special) {
          label = String(special);
        } else {
          label = String((char)key);
        }

        // Don't render placeholder blanks in sym row4
        if (key == ' ' && !(special && strcmp(special, "SPACE") == 0)) {
          continue;
        }

        if (r == keyRow && c == keyCol) {
          label = String(">") + label + String("<");
        }

        int x = startX + c * cellW;
        int y = startY + r * cellH;
        textRenderer.setCursor(x, y);
        textRenderer.print(label);
      }
    }
  }

  {
    textRenderer.setFont(&MenuFontSmall);
    textRenderer.setCursor(20, 780);
    textRenderer.print("Left/Right: Key  Vol+/Vol-: Row  OK: Select  Back: Cancel");
  }
}
