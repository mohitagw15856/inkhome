#include "AppRegistry.h"

#include <algorithm>
#include <array>
#include <cassert>

#include "apps/focus/FocusTimer.h"
#include "apps/fwswitch/FirmwareSwitchApp.h"
#include "apps/iffound/IfFoundApp.h"
#include "apps/inkrecipes/InkRecipesApp.h"
#include "apps/inkverse/InkVerseApp.h"
#include "apps/stats/ReadingStatsApp.h"

namespace inkhome::apps {

namespace {

// Launcher order. Add an app by including its header and appending its descriptor.
constexpr std::array<AppDescriptor, 6> APPS{{
    stats::DESCRIPTOR,
    focus::DESCRIPTOR,
    inkrecipes::DESCRIPTOR,
    inkverse::DESCRIPTOR,
    iffound::DESCRIPTOR,
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
