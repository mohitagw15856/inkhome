#include "BatterySaver.h"

#include <Arduino.h>
#include <HalGPIO.h>
#include <HalPowerManager.h>

#include "CrossPointSettings.h"

namespace inkhome::power {

namespace {

constexpr uint16_t SAVER_BELOW_PERCENT = 20;
constexpr unsigned long CHECK_EVERY_MS = 30000;
constexpr unsigned long SAVER_SLEEP_MS = 3UL * 60UL * 1000UL;

}  // namespace

bool saverActive() {
  static bool active = false;
  static unsigned long checkedAt = 0;
  static bool checked = false;
  if (!SETTINGS.batterySaver) return false;
  if (!checked || millis() - checkedAt >= CHECK_EVERY_MS) {
    checked = true;
    checkedAt = millis();
    active = !gpio.isUsbConnected() && powerManager.getBatteryPercentage() < SAVER_BELOW_PERCENT;
  }
  return active;
}

uint8_t tiltMode(const uint8_t configured) { return saverActive() ? 0 : configured; }

unsigned long sleepTimeoutMs(const unsigned long configured) {
  if (!saverActive()) return configured;
  return configured == 0 || configured > SAVER_SLEEP_MS ? SAVER_SLEEP_MS : configured;
}

int refreshFrequency(const int base) { return saverActive() ? base * 2 : base; }

}  // namespace inkhome::power
