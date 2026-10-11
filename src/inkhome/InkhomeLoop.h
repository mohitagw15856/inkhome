#pragma once

// One main-loop hook for every Inkhome background feature (reading stats,
// focus timer, motion gestures), so upstream's main.cpp carries a single line.
namespace inkhome {

void tick(bool readerActive, bool userActivity);
// Before deep sleep.
void beforeSleep();

}  // namespace inkhome
