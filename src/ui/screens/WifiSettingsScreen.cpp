#include "WifiSettingsScreen.h"

#include <resources/fonts/FontManager.h>
#include <resources/fonts/other/MenuFontSmall.h>
#include <resources/fonts/other/MenuHeader.h>

#include "../../core/Settings.h"
#include "../UIManager.h"

WifiSettingsScreen::WifiSettingsScreen(EInkDisplay& display, TextRenderer& renderer, UIManager& uiManager)
    : display(display), textRenderer(renderer), uiManager(uiManager) {}

void WifiSettingsScreen::begin() {
  loadSettings();
}

void WifiSettingsScreen::activate() {
  loadSettings();
}

void WifiSettingsScreen::handleButtons(Buttons& buttons) {
  if (buttons.isPressed(Buttons::BACK)) {
    saveSettings();
    uiManager.showScreen(UIManager::ScreenId::Settings);
  } else if (buttons.isPressed(Buttons::LEFT)) {
    selectNext();
  } else if (buttons.isPressed(Buttons::RIGHT)) {
    selectPrev();
  } else if (buttons.isPressed(Buttons::CONFIRM)) {
    activateSelection();
  }
}

void WifiSettingsScreen::show() {
  render();
  display.displayBuffer(EInkDisplay::FAST_REFRESH);
}

void WifiSettingsScreen::loadSettings() {
  Settings& s = uiManager.getSettings();

  int wifiEnabled = 0;
  if (s.getInt(String("wifi.enabled"), wifiEnabled)) {
    wifiEnabledIndex = wifiEnabled ? 1 : 0;
  }

  wifiSsid = s.getString(String("wifi.ssid"));
  wifiPass = s.getString(String("wifi.pass"));
}

void WifiSettingsScreen::saveSettings() {
  Settings& s = uiManager.getSettings();

  s.setInt(String("wifi.enabled"), wifiEnabledIndex);

  s.setString(String("wifi.ssid"), wifiSsid);
  s.setString(String("wifi.pass"), wifiPass);

  if (!s.save()) {
    Serial.println("WifiSettingsScreen: Failed to write settings.cfg");
  }
}

String WifiSettingsScreen::getItemName(int index) {
  switch (index) {
    case 0:
      return "WiFi";
    case 1:
      return "SSID";
    case 2:
      return "Password";
    default:
      return "";
  }
}

String WifiSettingsScreen::getItemValue(int index) {
  switch (index) {
    case 0:
      return wifiEnabledIndex ? "On" : "Off";
    case 1: {
      String v = wifiSsid;
      if (v.length() > 18)
        v = v.substring(0, 18) + "...";
      return v;
    }
    case 2: {
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
    default:
      return "";
  }
}

void WifiSettingsScreen::render() {
  display.clearScreen(0xFF);
  textRenderer.setTextColor(TextRenderer::COLOR_BLACK);
  textRenderer.setFont(getTitleFont());

  textRenderer.setFrameBuffer(display.getFrameBuffer());
  textRenderer.setBitmapType(TextRenderer::BITMAP_BW);

  uiManager.renderStatusHeader(textRenderer);

  textRenderer.setFont(getTitleFont());

  {
    const char* title = "WiFi Setup";
    int16_t x1, y1;
    uint16_t w, h;
    textRenderer.getTextBounds(title, 0, 0, &x1, &y1, &w, &h);
    int16_t centerX = (480 - (int)w) / 2;
    textRenderer.setCursor(centerX, 75);
    textRenderer.print(title);
  }

  textRenderer.setFont(getMainFont());

  const int lineHeight = 28;
  int totalHeight = ITEM_COUNT * lineHeight;
  int startY = (800 - totalHeight) / 2;

  for (int i = 0; i < ITEM_COUNT; ++i) {
    String line = getItemName(i);
    line += ": ";
    line += getItemValue(i);
    if (i == selectedIndex) {
      line = String(">") + line + String("<");
    }

    int16_t x1, y1;
    uint16_t w, h;
    textRenderer.getTextBounds(line.c_str(), 0, 0, &x1, &y1, &w, &h);
    int16_t centerX = (480 - (int)w) / 2;
    int16_t rowY = startY + i * lineHeight;
    textRenderer.setCursor(centerX, rowY);
    textRenderer.print(line);
  }
}

void WifiSettingsScreen::selectNext() {
  selectedIndex++;
  if (selectedIndex >= ITEM_COUNT)
    selectedIndex = 0;
  show();
}

void WifiSettingsScreen::selectPrev() {
  selectedIndex--;
  if (selectedIndex < 0)
    selectedIndex = ITEM_COUNT - 1;
  show();
}

void WifiSettingsScreen::activateSelection() {
  switch (selectedIndex) {
    case 0:
      wifiEnabledIndex = 1 - wifiEnabledIndex;
      saveSettings();
      show();
      break;
    case 1:
      saveSettings();
      uiManager.showScreen(UIManager::ScreenId::WifiSsidSelect);
      break;
    case 2:
      saveSettings();
      uiManager.showScreen(UIManager::ScreenId::WifiPasswordEntry);
      break;
  }
}
