#include "InkhomeLoop.h"

#include <GlyphBitmap.h>

#include "CrossPointSettings.h"
#include "apps/device/BatteryHealth.h"
#include "apps/focus/FocusTimer.h"
#include "apps/notes/NotesApp.h"
#include "apps/stats/ReadingStatsTracker.h"
#include "inkhome/Motion.h"

namespace inkhome {

void tick(const bool readerActive, const bool userActivity) {
  // The glyph painter reads this on the render task; a byte write is enough.
  glyphBitmap::textDarkness = SETTINGS.textDarkness > 2 ? 2 : SETTINGS.textDarkness;
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
