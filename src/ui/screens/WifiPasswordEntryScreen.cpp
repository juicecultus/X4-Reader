#include "WifiPasswordEntryScreen.h"

#include <resources/fonts/FontManager.h>
#include <resources/fonts/other/MenuFontSmall.h>
#include <resources/fonts/other/MenuHeader.h>

#include "../../core/Settings.h"
#include "../UIManager.h"

static const char* kKeysRow0[] = {"OK", "DEL", "SPACE", "-", "_", ".", "@"};
static const char* kKeysRow1[] = {"a", "b", "c", "d", "e", "f", "g", "h", "i", "j"};
static const char* kKeysRow2[] = {"k", "l", "m", "n", "o", "p", "q", "r", "s", "t"};
static const char* kKeysRow3[] = {"u", "v", "w", "x", "y", "z", "0", "1", "2", "3"};
static const char* kKeysRow4[] = {"4", "5", "6", "7", "8", "9", "A", "B", "C", "D"};
static const char* kKeysRow5[] = {"E", "F", "G", "H", "I", "J", "K", "L", "M", "N"};
static const char* kKeysRow6[] = {"O", "P", "Q", "R", "S", "T", "U", "V", "W", "X"};
static const char* kKeysRow7[] = {"Y", "Z"};

static const char** kKeyboardRows[] = {
    kKeysRow0, kKeysRow1, kKeysRow2, kKeysRow3, kKeysRow4, kKeysRow5, kKeysRow6, kKeysRow7,
};
static const int kKeyboardRowCounts[] = {7, 10, 10, 10, 10, 10, 10, 2};
static const int kKeyboardRowCount = 8;

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
}

void WifiPasswordEntryScreen::handleButtons(Buttons& buttons) {
  if (buttons.isPressed(Buttons::BACK)) {
    editBuffer = editOriginal;
    uiManager.showScreen(UIManager::ScreenId::WifiSettings);
  } else if (buttons.isPressed(Buttons::LEFT)) {
    int cols = kKeyboardRowCounts[keyRow];
    keyCol++;
    if (keyCol >= cols)
      keyCol = 0;
    show();
  } else if (buttons.isPressed(Buttons::RIGHT)) {
    int cols = kKeyboardRowCounts[keyRow];
    keyCol--;
    if (keyCol < 0)
      keyCol = cols - 1;
    show();
  } else if (buttons.isPressed(Buttons::VOLUME_UP)) {
    keyRow--;
    if (keyRow < 0)
      keyRow = kKeyboardRowCount - 1;
    int cols = kKeyboardRowCounts[keyRow];
    if (keyCol >= cols)
      keyCol = cols - 1;
    show();
  } else if (buttons.isPressed(Buttons::VOLUME_DOWN)) {
    keyRow++;
    if (keyRow >= kKeyboardRowCount)
      keyRow = 0;
    int cols = kKeyboardRowCounts[keyRow];
    if (keyCol >= cols)
      keyCol = cols - 1;
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
  const char* label = kKeyboardRows[keyRow][keyCol];
  if (!label)
    return;

  if (strcmp(label, "OK") == 0) {
    wifiPass = editBuffer;
    saveSettings();
    uiManager.showScreen(UIManager::ScreenId::WifiSettings);
    return;
  }

  if (strcmp(label, "DEL") == 0) {
    if (editBuffer.length() > 0) {
      editBuffer.remove(editBuffer.length() - 1);
    }
    show();
    return;
  }

  if (strcmp(label, "SPACE") == 0) {
    if (editBuffer.length() < 64) {
      editBuffer += ' ';
    }
    show();
    return;
  }

  // Regular key (single char)
  if (strlen(label) == 1 && editBuffer.length() < 64) {
    editBuffer += label[0];
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
      int cols = kKeyboardRowCounts[r];
      for (int c = 0; c < cols; ++c) {
        const char* key = kKeyboardRows[r][c];
        if (!key)
          continue;

        String label = String(key);
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
