#include "SettingsScreen.h"

#include <resources/fonts/FontDefinitions.h>
#include <resources/fonts/FontManager.h>
#include <resources/fonts/other/MenuFontBig.h>
#include <resources/fonts/other/MenuFontSmall.h>
#include <resources/fonts/other/MenuHeader.h>

#include "../../core/BatteryMonitor.h"
#include "../../core/Buttons.h"
#include "../../core/Settings.h"
#include "../UIManager.h"

constexpr int SettingsScreen::marginValues[];
constexpr int SettingsScreen::lineHeightValues[];

static const char* kTextChoices = "[OK][DEL] abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.@+/\\:";

SettingsScreen::SettingsScreen(EInkDisplay& display, TextRenderer& renderer, UIManager& uiManager)
    : display(display), textRenderer(renderer), uiManager(uiManager) {}

void SettingsScreen::begin() {
  loadSettings();
}

void SettingsScreen::handleButtons(Buttons& buttons) {
  if (editingText) {
    int choicesLen = (int)strlen(kTextChoices);
    if (buttons.isPressed(Buttons::BACK)) {
      editingText = false;
      editBuffer = editOriginal;
      show();
      return;
    } else if (buttons.isPressed(Buttons::LEFT)) {
      editChoiceIndex++;
      if (editChoiceIndex >= choicesLen)
        editChoiceIndex = 0;
      show();
      return;
    } else if (buttons.isPressed(Buttons::RIGHT)) {
      editChoiceIndex--;
      if (editChoiceIndex < 0)
        editChoiceIndex = choicesLen - 1;
      show();
      return;
    } else if (buttons.isPressed(Buttons::CONFIRM)) {
      // First 4 chars in kTextChoices encode [OK]
      if (editChoiceIndex >= 0 && editChoiceIndex <= 3) {
        // Commit
        if (editingKey == 0) {
          wifiSsid = editBuffer;
        } else {
          wifiPass = editBuffer;
        }
        editingText = false;
        saveSettings();
        if (wifiEnabledIndex) {
          uiManager.trySyncTimeFromNtp();
        }
        show();
        return;
      }
      // Next 5 chars encode [DEL]
      if (editChoiceIndex >= 4 && editChoiceIndex <= 8) {
        if (editBuffer.length() > 0) {
          editBuffer.remove(editBuffer.length() - 1);
        }
        show();
        return;
      }

      // Regular character
      char c = kTextChoices[editChoiceIndex];
      // Avoid adding bracket marker chars from [OK]/[DEL]
      if (c != '[' && c != ']' && c != 'O' && c != 'K' && c != 'D' && c != 'E' && c != 'L') {
        if (editBuffer.length() < 64) {
          editBuffer += c;
        }
      }
      show();
      return;
    }
  }

  if (buttons.isPressed(Buttons::BACK)) {
    saveSettings();
    // Return to the screen we came from
    uiManager.showScreen(uiManager.getPreviousScreen());
  } else if (buttons.isPressed(Buttons::LEFT)) {
    selectNext();
  } else if (buttons.isPressed(Buttons::RIGHT)) {
    selectPrev();
  } else if (buttons.isPressed(Buttons::CONFIRM)) {
    toggleCurrentSetting();
  }
}

void SettingsScreen::activate() {
  loadSettings();
}

void SettingsScreen::show() {
  renderSettings();
  display.displayBuffer(EInkDisplay::FAST_REFRESH);
}

void SettingsScreen::renderSettings() {
  display.clearScreen(0xFF);
  textRenderer.setTextColor(TextRenderer::COLOR_BLACK);
  textRenderer.setFont(getTitleFont());

  // Set framebuffer to BW buffer for rendering
  textRenderer.setFrameBuffer(display.getFrameBuffer());
  textRenderer.setBitmapType(TextRenderer::BITMAP_BW);

  // Center the title horizontally
  {
    const char* title = "Settings";
    int16_t x1, y1;
    uint16_t w, h;
    textRenderer.getTextBounds(title, 0, 0, &x1, &y1, &w, &h);
    int16_t centerX = (480 - (int)w) / 2;
    textRenderer.setCursor(centerX, 75);
    textRenderer.print(title);
  }

  textRenderer.setFont(getMainFont());

  // Render settings list
  const int lineHeight = 28;
  int totalHeight = SETTINGS_COUNT * lineHeight;
  int startY = (800 - totalHeight) / 2;  // center vertically

  for (int i = 0; i < SETTINGS_COUNT; ++i) {
    String displayName = getSettingName(i);
    displayName += ": ";
    displayName += getSettingValue(i);

    if (i == selectedIndex) {
      displayName = String(">") + displayName + String("<");
    }

    int16_t x1, y1;
    uint16_t w, h;
    textRenderer.getTextBounds(displayName.c_str(), 0, 0, &x1, &y1, &w, &h);
    int16_t centerX = (480 - (int)w) / 2;
    int16_t rowY = startY + i * lineHeight;
    textRenderer.setCursor(centerX, rowY);
    textRenderer.print(displayName);
  }

  // Draw battery percentage at bottom
  {
    textRenderer.setFont(&MenuFontSmall);  // Always use small font for battery
    int pct = g_battery.readPercentage();
    String pctStr = String(pct) + "%";
    int16_t bx1, by1;
    uint16_t bw, bh;
    textRenderer.getTextBounds(pctStr.c_str(), 0, 0, &bx1, &by1, &bw, &bh);
    int16_t bx = (480 - (int)bw) / 2;
    int16_t by = 790;
    textRenderer.setCursor(bx, by);
    textRenderer.print(pctStr);
  }
}

void SettingsScreen::selectNext() {
  if (editingText)
    return;
  selectedIndex++;
  if (selectedIndex >= SETTINGS_COUNT)
    selectedIndex = 0;
  show();
}

void SettingsScreen::selectPrev() {
  if (editingText)
    return;
  selectedIndex--;
  if (selectedIndex < 0)
    selectedIndex = SETTINGS_COUNT - 1;
  show();
}

void SettingsScreen::toggleCurrentSetting() {
  switch (selectedIndex) {
    case 0:  // Horizontal Margins
      marginIndex++;
      if (marginIndex >= marginValuesCount)
        marginIndex = 0;
      break;
    case 1:  // Line Height
      lineHeightIndex++;
      if (lineHeightIndex >= lineHeightValuesCount)
        lineHeightIndex = 0;
      break;
    case 2:  // Alignment
      alignmentIndex++;
      if (alignmentIndex >= 3)
        alignmentIndex = 0;
      break;
    case 3:  // Show Chapter Numbers
      showChapterNumbersIndex = 1 - showChapterNumbersIndex;
      break;
    case 4:  // Font Family
      fontFamilyIndex++;
      if (fontFamilyIndex >= 2)
        fontFamilyIndex = 0;
      applyFontSettings();
      break;
    case 5:  // Font Size
      fontSizeIndex++;
      if (fontSizeIndex >= 3)
        fontSizeIndex = 0;
      applyFontSettings();
      break;
    case 6:  // UI Font Size
      uiFontSizeIndex = 1 - uiFontSizeIndex;
      applyUIFontSettings();
      break;
    case 7:  // Random Sleep Cover
      randomSleepCoverIndex = 1 - randomSleepCoverIndex;
      break;
    case 8:  // WiFi
      wifiEnabledIndex = 1 - wifiEnabledIndex;
      break;
    case 9:  // Timezone
      tzOffsetHours++;
      if (tzOffsetHours > 14)
        tzOffsetHours = -12;
      break;
    case 10:  // WiFi SSID
      editingText = true;
      editingKey = 0;
      editOriginal = wifiSsid;
      editBuffer = wifiSsid;
      editChoiceIndex = 0;
      break;
    case 11:  // WiFi Password
      editingText = true;
      editingKey = 1;
      editOriginal = wifiPass;
      editBuffer = wifiPass;
      editChoiceIndex = 0;
      break;
    case 12:  // Clear Cache
      clearCacheStatus = uiManager.clearEpubCache() ? 1 : 0;
      break;
  }
  saveSettings();
  show();
}

void SettingsScreen::loadSettings() {
  Settings& s = uiManager.getSettings();

  // Load horizontal margins (applies to both left and right)
  int margin = 10;
  if (s.getInt(String("settings.margin"), margin)) {
    for (int i = 0; i < marginValuesCount; i++) {
      if (marginValues[i] == margin) {
        marginIndex = i;
        break;
      }
    }
  }

  // Load line height
  int lineHeight = 30;
  if (s.getInt(String("settings.lineHeight"), lineHeight)) {
    for (int i = 0; i < lineHeightValuesCount; i++) {
      if (lineHeightValues[i] == lineHeight) {
        lineHeightIndex = i;
        break;
      }
    }
  }

  // Load alignment
  int alignment = 0;
  if (s.getInt(String("settings.alignment"), alignment)) {
    alignmentIndex = alignment;
  }

  // Load show chapter numbers
  int showChapters = 1;
  if (s.getInt(String("settings.showChapterNumbers"), showChapters)) {
    showChapterNumbersIndex = showChapters;
  }

  // Load font family (0=NotoSans, 1=Bookerly)
  int fontFamily = 1;
  if (s.getInt(String("settings.fontFamily"), fontFamily)) {
    fontFamilyIndex = fontFamily;
  }

  // Load font size (0=Small, 1=Medium, 2=Large)
  int fontSize = 0;
  if (s.getInt(String("settings.fontSize"), fontSize)) {
    fontSizeIndex = fontSize;
  }

  // Load UI font size (0=Small/14, 1=Large/28)
  int uiFontSize = 0;
  if (s.getInt(String("settings.uiFontSize"), uiFontSize)) {
    uiFontSizeIndex = uiFontSize;
  }

  // Load random sleep cover (0=OFF, 1=ON)
  int randomSleepCover = 0;
  if (s.getInt(String("settings.randomSleepCover"), randomSleepCover)) {
    randomSleepCoverIndex = randomSleepCover;
  }

  // Load WiFi enabled (0=OFF, 1=ON)
  int wifiEnabled = 0;
  if (s.getInt(String("wifi.enabled"), wifiEnabled)) {
    wifiEnabledIndex = wifiEnabled ? 1 : 0;
  }

  wifiSsid = s.getString(String("wifi.ssid"));
  wifiPass = s.getString(String("wifi.pass"));

  // Load timezone offset in seconds (default 0)
  int gmtOffset = 0;
  if (s.getInt(String("wifi.gmtOffset"), gmtOffset)) {
    tzOffsetHours = gmtOffset / 3600;
    if (tzOffsetHours < -12)
      tzOffsetHours = -12;
    if (tzOffsetHours > 14)
      tzOffsetHours = 14;
  }

  // Apply the loaded font settings
  applyFontSettings();
  applyUIFontSettings();
}

void SettingsScreen::saveSettings() {
  Settings& s = uiManager.getSettings();

  s.setInt(String("settings.margin"), marginValues[marginIndex]);
  s.setInt(String("settings.lineHeight"), lineHeightValues[lineHeightIndex]);
  s.setInt(String("settings.alignment"), alignmentIndex);
  s.setInt(String("settings.showChapterNumbers"), showChapterNumbersIndex);
  s.setInt(String("settings.fontFamily"), fontFamilyIndex);
  s.setInt(String("settings.fontSize"), fontSizeIndex);
  s.setInt(String("settings.uiFontSize"), uiFontSizeIndex);
  s.setInt(String("settings.randomSleepCover"), randomSleepCoverIndex);

  s.setInt(String("wifi.enabled"), wifiEnabledIndex);
  s.setInt(String("wifi.gmtOffset"), tzOffsetHours * 3600);
  s.setInt(String("wifi.daylightOffset"), 0);

  s.setString(String("wifi.ssid"), wifiSsid);
  s.setString(String("wifi.pass"), wifiPass);

  if (!s.save()) {
    Serial.println("SettingsScreen: Failed to write settings.cfg");
  }
}

String SettingsScreen::getSettingName(int index) {
  switch (index) {
    case 0:
      return "Margins";
    case 1:
      return "Line Height";
    case 2:
      return "Alignment";
    case 3:
      return "Chapter Numbers";
    case 4:
      return "Font Family";
    case 5:
      return "Font Size";
    case 6:
      return "UI Font Size";
    case 7:
      return "Random Sleep Cover";
    case 8:
      return "WiFi";
    case 9:
      return "Timezone";
    case 10:
      return "WiFi SSID";
    case 11:
      return "WiFi Password";
    case 12:
      return "Clear Cache";
    default:
      return "";
  }
}

String SettingsScreen::getSettingValue(int index) {
  switch (index) {
    case 0:
      return String(marginValues[marginIndex]);
    case 1:
      return String(lineHeightValues[lineHeightIndex]);
    case 2:
      switch (alignmentIndex) {
        case 0:
          return "Left";
        case 1:
          return "Center";
        case 2:
          return "Right";
        default:
          return "Unknown";
      }
    case 3:
      return showChapterNumbersIndex ? "On" : "Off";
    case 4:
      switch (fontFamilyIndex) {
        case 0:
          return "NotoSans";
        case 1:
          return "Bookerly";
        default:
          return "Unknown";
      }
    case 5:
      switch (fontSizeIndex) {
        case 0:
          return "Small";
        case 1:
          return "Medium";
        case 2:
          return "Large";
        default:
          return "Unknown";
      }
    case 6:
      return uiFontSizeIndex ? "Large" : "Small";
    case 7:
      return randomSleepCoverIndex ? "On" : "Off";
    case 8:
      return wifiEnabledIndex ? "On" : "Off";
    case 9:
      {
        char buf[10];
        snprintf(buf, sizeof(buf), "UTC%+d", tzOffsetHours);
        return String(buf);
      }
    case 10:
      if (editingText && editingKey == 0) {
        String v = editBuffer;
        if (v.length() > 18)
          v = v.substring(0, 18) + "...";
        return v;
      }
      {
        String v = wifiSsid;
        if (v.length() == 0)
          v = "";
        if (v.length() > 18)
          v = v.substring(0, 18) + "...";
        return v;
      }
    case 11:
      if (editingText && editingKey == 1) {
        String v = editBuffer;
        if (v.length() > 18)
          v = v.substring(0, 18) + "...";
        return v;
      }
      {
        if (wifiPass.length() == 0)
          return "";
        int n = wifiPass.length();
        String stars;
        for (int i = 0; i < n && i < 12; ++i)
          stars += "*";
        if (n > 12)
          stars += "...";
        return stars;
      }
    case 12:
      if (clearCacheStatus < 0)
        return "";
      return clearCacheStatus ? "OK" : "FAIL";
    default:
      return "";
  }
}

void SettingsScreen::applyFontSettings() {
  // Determine which font family to use based on settings
  FontFamily* targetFamily = nullptr;

  if (fontFamilyIndex == 0) {  // NotoSans
    switch (fontSizeIndex) {
      case 0:
        targetFamily = &notoSans26Family;
        break;
      case 1:
        targetFamily = &notoSans28Family;
        break;
      case 2:
        targetFamily = &notoSans30Family;
        break;
    }
  } else if (fontFamilyIndex == 1) {  // Bookerly
    switch (fontSizeIndex) {
      case 0:
        targetFamily = &bookerly26Family;
        break;
      case 1:
        targetFamily = &bookerly28Family;
        break;
      case 2:
        targetFamily = &bookerly30Family;
        break;
    }
  }

  if (targetFamily) {
    setCurrentFontFamily(targetFamily);
  }
}

void SettingsScreen::applyUIFontSettings() {
  // Set main and title fonts for UI elements
  // Always use MenuHeader for headers/titles
  setTitleFont(&MenuHeader);

  if (uiFontSizeIndex == 0) {
    setMainFont(&MenuFontSmall);
  } else {
    setMainFont(&MenuFontBig);
  }
}
