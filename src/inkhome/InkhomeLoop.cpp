#include "InkhomeLoop.h"

#include <GlyphBitmap.h>
#include <HalTiltSensor.h>

#include "CrossPointSettings.h"
#include "apps/device/BatteryHealth.h"
#include "apps/focus/FocusTimer.h"
#include "apps/notes/NotesApp.h"
#include "apps/stats/ReadingStatsTracker.h"
#include "inkhome/Motion.h"
#include "inkhome/PageTurner.h"

namespace inkhome {

void tick(const bool readerActive, const bool userActivity) {
  // The glyph painter reads this on the render task; a byte write is enough.
  glyphBitmap::textDarkness = SETTINGS.textDarkness > 2 ? 2 : SETTINGS.textDarkness;
#ifndef SIMULATOR  // the simulator's tilt stub has no sensitivity
  halTiltSensor.setSensitivity(SETTINGS.tiltSensitivity);
#endif
  stats::tick(readerActive, userActivity);
  focus::tick(readerActive);
  motion::tick(readerActive, userActivity);
  pageturner::tick(readerActive);
}

void beforeSleep() {
  stats::flush();
  battery::logDaily(stats::todayOrdinal());
  notes::exportCurrentBook();
}

}  // namespace inkhome
