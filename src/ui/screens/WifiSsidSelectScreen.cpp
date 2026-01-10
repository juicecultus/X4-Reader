#include "WifiSsidSelectScreen.h"

#include <resources/fonts/FontManager.h>
#include <resources/fonts/other/MenuFontSmall.h>
#include <resources/fonts/other/MenuHeader.h>

#include <WiFi.h>

#include "../../core/Settings.h"
#include "../UIManager.h"

WifiSsidSelectScreen::WifiSsidSelectScreen(EInkDisplay& display, TextRenderer& renderer, UIManager& uiManager)
    : display(display), textRenderer(renderer), uiManager(uiManager) {}

void WifiSsidSelectScreen::begin() {}

void WifiSsidSelectScreen::activate() {
  scanNetworks();
}

void WifiSsidSelectScreen::handleButtons(Buttons& buttons) {
  if (buttons.isPressed(Buttons::BACK)) {
    uiManager.showScreen(UIManager::ScreenId::WifiSettings);
  } else if (buttons.isPressed(Buttons::LEFT)) {
    selectNext();
  } else if (buttons.isPressed(Buttons::RIGHT)) {
    selectPrev();
  } else if (buttons.isPressed(Buttons::CONFIRM)) {
    confirm();
  }
}

void WifiSsidSelectScreen::show() {
  render();
  display.displayBuffer(EInkDisplay::FAST_REFRESH);
}

void WifiSsidSelectScreen::scanNetworks() {
  ssids.clear();
  scanning = true;
  selectedIndex = 0;
  scrollOffset = 0;

  show();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(50);

  int n = WiFi.scanNetworks(false, true);
  if (n < 0)
    n = 0;

  for (int i = 0; i < n; ++i) {
    String s = WiFi.SSID(i);
    if (s.length() == 0)
      continue;
    // De-dupe
    bool dup = false;
    for (auto const& existing : ssids) {
      if (existing == s) {
        dup = true;
        break;
      }
    }
    if (!dup)
      ssids.push_back(s);
  }

  WiFi.scanDelete();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  scanning = false;
  show();
}

void WifiSsidSelectScreen::render() {
  display.clearScreen(0xFF);
  textRenderer.setTextColor(TextRenderer::COLOR_BLACK);
  textRenderer.setFont(getTitleFont());

  textRenderer.setFrameBuffer(display.getFrameBuffer());
  textRenderer.setBitmapType(TextRenderer::BITMAP_BW);

  uiManager.renderStatusHeader(textRenderer);

  textRenderer.setFont(getTitleFont());

  {
    const char* title = "Select SSID";
    int16_t x1, y1;
    uint16_t w, h;
    textRenderer.getTextBounds(title, 0, 0, &x1, &y1, &w, &h);
    int16_t centerX = (480 - (int)w) / 2;
    textRenderer.setCursor(centerX, 75);
    textRenderer.print(title);
  }

  textRenderer.setFont(getMainFont());

  if (scanning) {
    textRenderer.setCursor(40, 200);
    textRenderer.print("Scanning...");
    return;
  }

  if (ssids.empty()) {
    textRenderer.setCursor(40, 200);
    textRenderer.print("No networks");
    textRenderer.setCursor(40, 240);
    textRenderer.print("Press BACK");
    return;
  }

  const int lineHeight = 28;
  const int linesPerScreen = 10;
  if (selectedIndex < scrollOffset)
    scrollOffset = selectedIndex;
  if (selectedIndex >= scrollOffset + linesPerScreen)
    scrollOffset = selectedIndex - linesPerScreen + 1;

  int startY = 140;
  for (int i = 0; i < linesPerScreen; ++i) {
    int idx = scrollOffset + i;
    if (idx >= (int)ssids.size())
      break;

    String name = ssids[idx];
    if (name.length() > 28)
      name = name.substring(0, 28) + "...";

    if (idx == selectedIndex) {
      name = String(">") + name + String("<");
    }

    textRenderer.setCursor(20, startY + i * lineHeight);
    textRenderer.print(name);
  }
}

void WifiSsidSelectScreen::selectNext() {
  if (ssids.empty())
    return;
  selectedIndex++;
  if (selectedIndex >= (int)ssids.size())
    selectedIndex = 0;
  show();
}

void WifiSsidSelectScreen::selectPrev() {
  if (ssids.empty())
    return;
  selectedIndex--;
  if (selectedIndex < 0)
    selectedIndex = (int)ssids.size() - 1;
  show();
}

void WifiSsidSelectScreen::confirm() {
  if (ssids.empty())
    return;

  Settings& s = uiManager.getSettings();
  s.setString(String("wifi.ssid"), ssids[selectedIndex]);
  if (!s.save()) {
    Serial.println("WifiSsidSelectScreen: Failed to write settings.cfg");
  }

  uiManager.showScreen(UIManager::ScreenId::WifiSettings);
}
