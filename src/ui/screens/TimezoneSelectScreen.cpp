#include "TimezoneSelectScreen.h"

#include <resources/fonts/FontManager.h>
#include <resources/fonts/other/MenuFontSmall.h>
#include <resources/fonts/other/MenuHeader.h>

#include "../../core/Buttons.h"
#include "../../core/Settings.h"
#include "../UIManager.h"

struct TzCity {
  const char* name;
  int8_t utcOffsetHours;
};

struct TzCountry {
  const char* name;
  const TzCity* cities;
  int cityCount;
};

struct TzContinent {
  const char* name;
  const TzCountry* countries;
  int countryCount;
};

static const TzCity kCitiesUK[] = {{"London", 0}};
static const TzCity kCitiesFrance[] = {{"Paris", 1}};
static const TzCity kCitiesGermany[] = {{"Berlin", 1}};
static const TzCity kCitiesSpain[] = {{"Madrid", 1}};
static const TzCity kCitiesItaly[] = {{"Rome", 1}};
static const TzCity kCitiesNetherlands[] = {{"Amsterdam", 1}};

static const TzCountry kCountriesEurope[] = {
    {"United Kingdom", kCitiesUK, 1},
    {"France", kCitiesFrance, 1},
    {"Germany", kCitiesGermany, 1},
    {"Spain", kCitiesSpain, 1},
    {"Italy", kCitiesItaly, 1},
    {"Netherlands", kCitiesNetherlands, 1},
};

static const TzCity kCitiesUSA[] = {{"Washington, D.C.", -5}};
static const TzCity kCitiesCanada[] = {{"Ottawa", -5}};
static const TzCountry kCountriesNorthAmerica[] = {
    {"United States", kCitiesUSA, 1},
    {"Canada", kCitiesCanada, 1},
};

static const TzCity kCitiesJapan[] = {{"Tokyo", 9}};
static const TzCity kCitiesIndia[] = {{"New Delhi", 5}};
static const TzCity kCitiesChina[] = {{"Beijing", 8}};
static const TzCountry kCountriesAsia[] = {
    {"Japan", kCitiesJapan, 1},
    {"India", kCitiesIndia, 1},
    {"China", kCitiesChina, 1},
};

static const TzCity kCitiesAustralia[] = {{"Canberra", 10}};
static const TzCity kCitiesNewZealand[] = {{"Wellington", 12}};
static const TzCountry kCountriesOceania[] = {
    {"Australia", kCitiesAustralia, 1},
    {"New Zealand", kCitiesNewZealand, 1},
};

static const TzContinent kContinents[] = {
    {"Europe", kCountriesEurope, (int)(sizeof(kCountriesEurope) / sizeof(kCountriesEurope[0]))},
    {"North America", kCountriesNorthAmerica, (int)(sizeof(kCountriesNorthAmerica) / sizeof(kCountriesNorthAmerica[0]))},
    {"Asia", kCountriesAsia, (int)(sizeof(kCountriesAsia) / sizeof(kCountriesAsia[0]))},
    {"Oceania", kCountriesOceania, (int)(sizeof(kCountriesOceania) / sizeof(kCountriesOceania[0]))},
};

static const int kContinentCount = (int)(sizeof(kContinents) / sizeof(kContinents[0]));

TimezoneSelectScreen::TimezoneSelectScreen(EInkDisplay& display, TextRenderer& renderer, UIManager& uiManager)
    : display(display), textRenderer(renderer), uiManager(uiManager) {}

void TimezoneSelectScreen::begin() {}

void TimezoneSelectScreen::activate() {
  level = Level::Continent;
  selectedIndex = 0;
  selectedContinent = -1;
  selectedCountry = -1;
}

void TimezoneSelectScreen::handleButtons(Buttons& buttons) {
  if (buttons.isPressed(Buttons::BACK)) {
    goBack();
  } else if (buttons.isPressed(Buttons::LEFT)) {
    selectNext();
  } else if (buttons.isPressed(Buttons::RIGHT)) {
    selectPrev();
  } else if (buttons.isPressed(Buttons::CONFIRM)) {
    activateSelection();
  }
}

void TimezoneSelectScreen::show() {
  render();
  display.displayBuffer(EInkDisplay::FAST_REFRESH);
}

int TimezoneSelectScreen::getItemCount() const {
  if (level == Level::Continent) {
    return kContinentCount;
  }
  if (level == Level::Country) {
    if (selectedContinent < 0 || selectedContinent >= kContinentCount)
      return 0;
    return kContinents[selectedContinent].countryCount;
  }
  if (level == Level::City) {
    if (selectedContinent < 0 || selectedContinent >= kContinentCount)
      return 0;
    const TzContinent& cont = kContinents[selectedContinent];
    if (selectedCountry < 0 || selectedCountry >= cont.countryCount)
      return 0;
    return cont.countries[selectedCountry].cityCount;
  }
  return 0;
}

String TimezoneSelectScreen::getTitle() const {
  if (level == Level::Continent) {
    return "Timezone";
  }
  if (level == Level::Country) {
    return String(kContinents[selectedContinent].name);
  }
  if (level == Level::City) {
    const TzContinent& cont = kContinents[selectedContinent];
    return String(cont.countries[selectedCountry].name);
  }
  return "Timezone";
}

String TimezoneSelectScreen::getItemLabel(int index) const {
  if (level == Level::Continent) {
    if (index < 0 || index >= kContinentCount)
      return "";
    return String(kContinents[index].name);
  }
  if (level == Level::Country) {
    const TzContinent& cont = kContinents[selectedContinent];
    if (index < 0 || index >= cont.countryCount)
      return "";
    return String(cont.countries[index].name);
  }
  if (level == Level::City) {
    const TzContinent& cont = kContinents[selectedContinent];
    const TzCountry& ctry = cont.countries[selectedCountry];
    if (index < 0 || index >= ctry.cityCount)
      return "";
    const TzCity& city = ctry.cities[index];
    char buf[32];
    snprintf(buf, sizeof(buf), "UTC%+d %s", (int)city.utcOffsetHours, city.name);
    return String(buf);
  }
  return "";
}

void TimezoneSelectScreen::render() {
  display.clearScreen(0xFF);
  textRenderer.setTextColor(TextRenderer::COLOR_BLACK);
  textRenderer.setFont(getTitleFont());

  textRenderer.setFrameBuffer(display.getFrameBuffer());
  textRenderer.setBitmapType(TextRenderer::BITMAP_BW);

  uiManager.renderStatusHeader(textRenderer);

  textRenderer.setFont(getTitleFont());

  String title = getTitle();
  {
    int16_t x1, y1;
    uint16_t w, h;
    textRenderer.getTextBounds(title.c_str(), 0, 0, &x1, &y1, &w, &h);
    int16_t centerX = (480 - (int)w) / 2;
    textRenderer.setCursor(centerX, 75);
    textRenderer.print(title);
  }

  textRenderer.setFont(getMainFont());

  int count = getItemCount();
  if (count <= 0) {
    return;
  }

  const int lineHeight = 28;
  int maxLines = 16;
  if (count < maxLines)
    maxLines = count;

  int startIndex = 0;
  if (selectedIndex >= maxLines) {
    startIndex = selectedIndex - (maxLines - 1);
  }

  int totalHeight = maxLines * lineHeight;
  int startY = (800 - totalHeight) / 2;

  for (int i = 0; i < maxLines; ++i) {
    int idx = startIndex + i;
    String line = getItemLabel(idx);
    if (idx == selectedIndex) {
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

void TimezoneSelectScreen::selectNext() {
  int count = getItemCount();
  if (count <= 0)
    return;
  selectedIndex++;
  if (selectedIndex >= count)
    selectedIndex = 0;
  show();
}

void TimezoneSelectScreen::selectPrev() {
  int count = getItemCount();
  if (count <= 0)
    return;
  selectedIndex--;
  if (selectedIndex < 0)
    selectedIndex = count - 1;
  show();
}

void TimezoneSelectScreen::goBack() {
  if (level == Level::City) {
    level = Level::Country;
    selectedIndex = selectedCountry >= 0 ? selectedCountry : 0;
    show();
    return;
  }
  if (level == Level::Country) {
    level = Level::Continent;
    selectedIndex = selectedContinent >= 0 ? selectedContinent : 0;
    selectedCountry = -1;
    show();
    return;
  }

  uiManager.showScreen(UIManager::ScreenId::ClockSettings);
}

void TimezoneSelectScreen::activateSelection() {
  if (level == Level::Continent) {
    selectedContinent = selectedIndex;
    level = Level::Country;
    selectedIndex = 0;
    show();
    return;
  }
  if (level == Level::Country) {
    selectedCountry = selectedIndex;
    level = Level::City;
    selectedIndex = 0;
    show();
    return;
  }
  if (level == Level::City) {
    const TzContinent& cont = kContinents[selectedContinent];
    const TzCountry& ctry = cont.countries[selectedCountry];
    const TzCity& city = ctry.cities[selectedIndex];

    saveTimezoneSelection(String(cont.name), String(ctry.name), String(city.name), (int)city.utcOffsetHours);
    uiManager.showScreen(UIManager::ScreenId::ClockSettings);
    return;
  }
}

void TimezoneSelectScreen::saveTimezoneSelection(const String& continent, const String& country, const String& city,
                                                int tzOffsetHours) {
  Settings& s = uiManager.getSettings();
  s.setString(String("clock.tz.continent"), continent);
  s.setString(String("clock.tz.country"), country);
  s.setString(String("clock.tz.city"), city);

  s.setInt(String("wifi.gmtOffset"), tzOffsetHours * 3600);
  s.setInt(String("wifi.daylightOffset"), 0);

  if (!s.save()) {
    Serial.println("TimezoneSelectScreen: Failed to write settings.cfg");
  }
}
