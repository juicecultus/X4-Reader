#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include <Arduino.h>
#include <memory>
#include <array>

#include "core/Buttons.h"
#include "core/EInkDisplay.h"
#include "rendering/TextRenderer.h"
#include "text/layout/LayoutStrategy.h"
#include "ui/screens/Screen.h"

class SDCardManager;

// Forward-declare concrete screen types (global, not nested)
class FileBrowserScreen;
class ImageViewerScreen;
class TextViewerScreen;

class Settings;

class UIManager {
 public:
  // Typed screen identifiers so callers don't use raw indices
  enum class ScreenId { FileBrowser, ImageViewer, TextViewer };

  static constexpr size_t kScreenCount = 3;

  static constexpr size_t toIndex(ScreenId id) {
    return static_cast<size_t>(id);
  }

  // Constructor
  UIManager(EInkDisplay& display, class SDCardManager& sdManager);
  ~UIManager();

  void begin();
  void handleButtons(Buttons& buttons);
  void showSleepScreen();
  // Prepare UI for power-off: notify active screen to persist state
  void prepareForSleep();

  // Show a screen by id
  void showScreen(ScreenId id);

  // Open a text file (path on SD) in the text viewer and switch to that screen.
  void openTextFile(const String& sdPath);

 private:
  EInkDisplay& display;
  SDCardManager& sdManager;
  TextRenderer textRenderer;

  ScreenId currentScreen = ScreenId::FileBrowser;

  // Fixed-size owning pointers to screens. Avoids unordered_map/rehash code paths
  // that can crash on ESP32-C3.
  std::array<std::unique_ptr<Screen>, kScreenCount> screens;

  // Global settings manager (single consolidated settings file)
  class Settings* settings = nullptr;

 public:
  Settings& getSettings() {
    return *settings;
  }
};

#endif
