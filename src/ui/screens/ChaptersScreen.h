#ifndef CHAPTERS_SCREEN_H
#define CHAPTERS_SCREEN_H

#include <Arduino.h>

#include "../../core/EInkDisplay.h"
#include "../../rendering/TextRenderer.h"
#include "Screen.h"

class UIManager;

class ChaptersScreen : public Screen {
 public:
  ChaptersScreen(EInkDisplay& display, TextRenderer& renderer, UIManager& uiManager);

  void begin() override {}
  void activate() override;
  void show() override;
  void handleButtons(class Buttons& buttons) override;

 private:
  void render();
  void selectNext();
  void selectPrev();
  void confirm();

  int getChapterCount() const;
  String getChapterLabel(int index) const;

  EInkDisplay& display;
  TextRenderer& textRenderer;
  UIManager& uiManager;

  int selectedIndex = 0;
};

#endif
