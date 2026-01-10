#include "ClockSettingsScreen.h"

#include <resources/fonts/FontManager.h>
#include <resources/fonts/other/MenuFontSmall.h>
#include <resources/fonts/other/MenuHeader.h>

#include "../../core/Buttons.h"
#include "../../core/Settings.h"
#include "../UIManager.h"

ClockSettingsScreen::ClockSettingsScreen(EInkDisplay& display, TextRenderer& renderer, UIManager& uiManager)
    : display(display), textRenderer(renderer), uiManager(uiManager) {}

void ClockSettingsScreen::begin() {}

void ClockSettingsScreen::activate() {}

void ClockSettingsScreen::handleButtons(Buttons& buttons) {
  if (buttons.isPressed(Buttons::BACK)) {
    uiManager.showScreen(UIManager::ScreenId::Settings);
  } else if (buttons.isPressed(Buttons::LEFT)) {
    selectNext();
  } else if (buttons.isPressed(Buttons::RIGHT)) {
    selectPrev();
  } else if (buttons.isPressed(Buttons::CONFIRM)) {
    activateSelection();
  }
}

void ClockSettingsScreen::show() {
  render();
  display.displayBuffer(EInkDisplay::FAST_REFRESH);
}

String ClockSettingsScreen::getItemName(int index) {
  switch (index) {
    case 0:
      return "Timezone";
    case 1:
      return "Sync Now";
    default:
      return "";
  }
}

String ClockSettingsScreen::getItemValue(int index) {
  Settings& s = uiManager.getSettings();
  switch (index) {
    case 0: {
      String city = s.getString(String("clock.tz.city"));
      if (city.length() > 0) {
        return city;
      }
      int gmtOffset = 0;
      (void)s.getInt(String("wifi.gmtOffset"), gmtOffset);
      int tzOffsetHours = gmtOffset / 3600;
      char buf[10];
      snprintf(buf, sizeof(buf), "UTC%+d", tzOffsetHours);
      return String(buf);
    }
    case 1:
      return "";
    default:
      return "";
  }
}

void ClockSettingsScreen::render() {
  display.clearScreen(0xFF);
  textRenderer.setTextColor(TextRenderer::COLOR_BLACK);
  textRenderer.setFont(getTitleFont());

  textRenderer.setFrameBuffer(display.getFrameBuffer());
  textRenderer.setBitmapType(TextRenderer::BITMAP_BW);

  uiManager.renderStatusHeader(textRenderer);

  textRenderer.setFont(getTitleFont());

  {
    const char* title = "Clock";
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

void ClockSettingsScreen::selectNext() {
  selectedIndex++;
  if (selectedIndex >= ITEM_COUNT)
    selectedIndex = 0;
  show();
}

void ClockSettingsScreen::selectPrev() {
  selectedIndex--;
  if (selectedIndex < 0)
    selectedIndex = ITEM_COUNT - 1;
  show();
}

void ClockSettingsScreen::activateSelection() {
  switch (selectedIndex) {
    case 0:
      uiManager.showScreen(UIManager::ScreenId::TimezoneSelect);
      break;
    case 1: {
      Settings& s = uiManager.getSettings();
      int wifiEnabled = 0;
      (void)s.getInt(String("wifi.enabled"), wifiEnabled);
      if (wifiEnabled) {
        uiManager.trySyncTimeFromNtp();
      }
      show();
      break;
    }
  }
}
