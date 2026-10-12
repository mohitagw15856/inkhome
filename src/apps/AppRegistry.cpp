#include "AppRegistry.h"

#include <algorithm>
#include <array>
#include <cassert>

#include "apps/chessink/ChessInkApp.h"
#include "apps/dailydrop/DailyDropApp.h"
#include "apps/device/BatteryHealth.h"
#include "apps/device/MotionTest.h"
#include "apps/focus/FocusTimer.h"
#include "apps/fwswitch/FirmwareSwitchApp.h"
#include "apps/habits/HabitsApp.h"
#include "apps/iffound/IfFoundApp.h"
#include "apps/inkcards/InkcardsApp.h"
#include "apps/inkrecipes/InkRecipesApp.h"
#include "apps/inkverse/InkVerseApp.h"
#include "apps/notes/NotesApp.h"
#include "apps/stats/AchievementsApp.h"
#include "apps/stats/ReadingStatsApp.h"
#include "apps/stats/YearInBooksApp.h"
#include "apps/storyscapes/StoryScapesApp.h"

namespace inkhome::apps {

namespace {

// Launcher order. Add an app by including its header and appending its descriptor.
constexpr std::array<AppDescriptor, 16> APPS{{
    stats::DESCRIPTOR,
    stats::YEAR_DESCRIPTOR,
    stats::ACHIEVEMENTS_DESCRIPTOR,
    notes::DESCRIPTOR,
    focus::DESCRIPTOR,
    habits::DESCRIPTOR,
    inkcards::DESCRIPTOR,
    chessink::DESCRIPTOR,
    inkrecipes::DESCRIPTOR,
    inkverse::DESCRIPTOR,
    storyscapes::DESCRIPTOR,
    dailydrop::DESCRIPTOR,
    iffound::DESCRIPTOR,
    battery::DESCRIPTOR,
    motion::TEST_DESCRIPTOR,
    fwswitch::DESCRIPTOR,
}};

bool isAvailable(const AppDescriptor& app) { return app.isAvailable == nullptr || app.isAvailable(); }

}  // namespace

size_t availableCount() { return static_cast<size_t>(std::count_if(APPS.begin(), APPS.end(), isAvailable)); }

const AppDescriptor& availableAt(const size_t index) {
  size_t seen = 0;
  for (const auto& app : APPS) {
    if (!isAvailable(app)) continue;
    if (seen++ == index) return app;
  }
  // Callers index below availableCount(); an out-of-range index is a programming error.
  assert(false);
  return APPS[0];
}

}  // namespace inkhome::apps
