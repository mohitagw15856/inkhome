#pragma once

// The home screen's reading-goal line: minutes left today against the daily
// goal (Settings, Reader, Daily goal), or a note that it is reached. Empty
// when the clock is not set, the stats cannot load, or the line is off.

#include <string>

namespace inkhome {

std::string goalLine();

}  // namespace inkhome
