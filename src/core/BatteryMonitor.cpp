// BatteryMonitor.cpp
#include "BatteryMonitor.h"
#include "esp_adc_cal.h"

#ifdef USE_M5UNIFIED
#include <M5Unified.h>
#endif

BatteryMonitor::BatteryMonitor(uint8_t adcPin, uint16_t dividerMultiplier100)
    : _adcPin(adcPin), _dividerMultiplier100(dividerMultiplier100) {
}

uint16_t BatteryMonitor::readPercentage() const {
#ifdef USE_M5UNIFIED
  return (uint16_t)M5.Power.getBatteryLevel();
#else
  return percentageFromMillivolts(readMillivolts());
#endif
}

uint16_t BatteryMonitor::readMillivolts() const {
#ifdef USE_M5UNIFIED
  return (uint16_t)(M5.Power.getBatteryVoltage() * 1000.0f);
#else
  const uint16_t raw = readRawMillivolts();
  const uint32_t mv = millivoltsFromRawAdc(raw);
  return static_cast<uint16_t>((mv * _dividerMultiplier100) / 100);
#endif
}

uint16_t BatteryMonitor::readRawMillivolts() const {
  const uint16_t raw = analogRead(_adcPin);
  return raw;
}

uint16_t BatteryMonitor::percentageFromMillivolts(uint16_t millivolts) {
  // Use integer-based calculation for battery percentage
  // V is voltage in millivolts
  // Typical LiPo range: 4200mV (100%) down to 3300mV (0%)
  if (millivolts >= 4200) return 100;
  if (millivolts <= 3300) return 0;

  // Simple linear mapping: (mv - 3300) / (4200 - 3300) * 100
  return (uint16_t)((millivolts - 3300) * 100 / 900);
}

uint16_t BatteryMonitor::millivoltsFromRawAdc(uint16_t adc_raw)
{
  esp_adc_cal_characteristics_t adc_chars;
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_12, ADC_WIDTH_BIT_12, 1100, &adc_chars);
  return esp_adc_cal_raw_to_voltage(adc_raw, &adc_chars);
}
