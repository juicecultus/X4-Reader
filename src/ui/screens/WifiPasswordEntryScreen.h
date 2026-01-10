#ifndef WIFI_PASSWORD_ENTRY_SCREEN_H
#define WIFI_PASSWORD_ENTRY_SCREEN_H

#include <Arduino.h>

#include "../../core/EInkDisplay.h"
#include "../../rendering/TextRenderer.h"
#include "Screen.h"

class Buttons;
class UIManager;

class WifiPasswordEntryScreen : public Screen {
 public:
  WifiPasswordEntryScreen(EInkDisplay& display, TextRenderer& renderer, UIManager& uiManager);

  void begin() override;
  void handleButtons(Buttons& buttons) override;
  void activate() override;
  void show() override;
  void shutdown() override {}

 private:
  EInkDisplay& display;
  TextRenderer& textRenderer;
  UIManager& uiManager;

  String wifiPass;
  String editOriginal;
  String editBuffer;
  int keyRow = 0;
  int keyCol = 0;

  void loadSettings();
  void saveSettings();
  void render();

  void chooseKey();
};

#endif
