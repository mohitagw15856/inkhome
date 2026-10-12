#pragma once

// Battery health from the X3's BQ27220 fuel gauge: state of health, full-charge
// versus design capacity, charge cycles, temperature and current, plus a daily
// log (/.crosspoint/apps/battery/history.csv) charted on screen. Read-only on
// the gauge. Only listed where the gauge exists (X3); the X4 measures voltage only.

#include <cstdint>
#include <memory>

#include "activities/Activity.h"
#include "apps/AppRegistry.h"

namespace inkhome::battery {

struct GaugeReading {
  bool valid = false;
  uint16_t percent = 0;
  uint16_t millivolts = 0;
  int16_t currentMa = 0;  // positive: charging
  uint16_t remainingMah = 0;
  uint16_t fullChargeMah = 0;
  uint16_t designMah = 0;
  uint16_t cycles = 0;
  uint16_t healthPercent = 0;
  int16_t tenthsC = 0;
};

bool hasGauge();
GaugeReading read();
// Appends today's reading to the history log once per day (called before sleep).
void logDaily(uint32_t dayOrdinal);

std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

constexpr apps::AppDescriptor DESCRIPTOR{"battery", StrId::STR_BATTERY_TITLE, StrId::STR_BATTERY_SUMMARY, &create,
                                         &hasGauge};

class BatteryHealthActivity final : public Activity {
 public:
  explicit BatteryHealthActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("BatteryHealth", renderer, mappedInput) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  GaugeReading reading;
  unsigned long lastReadMs = 0;
};

}  // namespace inkhome::battery
