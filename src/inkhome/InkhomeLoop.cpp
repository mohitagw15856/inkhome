#include "InkhomeLoop.h"

#include "apps/device/BatteryHealth.h"
#include "apps/focus/FocusTimer.h"
#include "apps/notes/NotesApp.h"
#include "apps/stats/ReadingStatsTracker.h"
#include "inkhome/Motion.h"

namespace inkhome {

void tick(const bool readerActive, const bool userActivity) {
  stats::tick(readerActive, userActivity);
  focus::tick(readerActive);
  motion::tick(readerActive, userActivity);
}

void beforeSleep() {
  stats::flush();
  battery::logDaily(stats::todayOrdinal());
  notes::exportCurrentBook();
}

}  // namespace inkhome
