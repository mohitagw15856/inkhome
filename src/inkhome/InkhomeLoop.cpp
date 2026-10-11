#include "InkhomeLoop.h"

#include "apps/focus/FocusTimer.h"
#include "apps/stats/ReadingStatsTracker.h"

namespace inkhome {

void tick(const bool readerActive, const bool userActivity) {
  stats::tick(readerActive, userActivity);
  focus::tick(readerActive);
}

void beforeSleep() { stats::flush(); }

}  // namespace inkhome
