#include "GoalLine.h"

#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "CrossPointSettings.h"
#include "apps/stats/ReadingStatsTracker.h"

namespace inkhome {

namespace {

constexpr uint64_t GOAL_MINUTES[] = {15, 30, 45, 60};  // dailyGoalTarget order

}  // namespace

std::string goalLine() {
  if (!SETTINGS.homeGoalLine) return {};
  const uint32_t today = stats::todayOrdinal();
  if (today == 0 || stats::ensureLoaded() != stats::LoadState::Ready) return {};
  uint64_t todayMs = stats::pendingMs();
  const auto& days = stats::data().readingDays;
  const auto it = std::find_if(days.begin(), days.end(), [today](const auto& d) { return d.dayOrdinal == today; });
  if (it != days.end()) todayMs += it->readingMs;

  const uint64_t goalMs = GOAL_MINUTES[SETTINGS.dailyGoalTarget < 4 ? SETTINGS.dailyGoalTarget : 1] * 60000ULL;
  if (todayMs >= goalMs) return tr(STR_GOAL_DONE);
  const unsigned long minutesLeft = static_cast<unsigned long>((goalMs - todayMs + 59999ULL) / 60000ULL);
  char line[64];
  snprintf(line, sizeof(line), "%lu %s", minutesLeft, tr(STR_GOAL_MIN_LEFT));
  return line;
}

}  // namespace inkhome
