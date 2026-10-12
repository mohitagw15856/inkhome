#include "QuietHours.h"

#include <HalClock.h>

#include <limits>

#include "CrossPointSettings.h"

namespace inkhome {

namespace {
constexpr int START_HOUR = 22;
constexpr int END_HOUR = 7;
constexpr int NIGHT_FACTOR = 3;
}  // namespace

bool quietHoursActive() {
  if (!SETTINGS.quietHours) return false;
  uint8_t hour = 0;
  uint8_t minute = 0;
  if (!halClock.getTime(hour, minute)) return false;
  return hour >= START_HOUR || hour < END_HOUR;
}

int quietHoursRefreshFrequency(const int base) {
  if (!quietHoursActive() || base >= std::numeric_limits<int>::max() / NIGHT_FACTOR) return base;
  return base * NIGHT_FACTOR;
}

}  // namespace inkhome
