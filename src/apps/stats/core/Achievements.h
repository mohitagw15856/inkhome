#pragma once

// Achievements compatible with CPR-vCodex's /.crosspoint/achievements.json
// (formatVersion 2): the 62 badges in vCodex's exact order (its file stores
// states by position), with the same metrics and targets. Progress is worked
// out from reading_stats.json; bookmark badges only carry over from an
// imported file, since Inkhome does not count bookmarks. Pure C++, host-tested.
// Definitions from CPR-vCodex (franssjz/cpr-vcodex, AchievementsStore), MIT.

#include <ArduinoJson.h>

#include <array>
#include <cstdint>
#include <vector>

#include "ReadingStatsData.h"

namespace inkhome::stats {

enum class Metric : uint8_t { BooksStarted, BooksFinished, Sessions, TotalReadingMs, GoalDays, MaxGoalStreak,
                              Bookmarks, MaxSessionMs };

struct AchievementDef {
  Metric metric;
  uint64_t target;
};

constexpr size_t ACHIEVEMENT_COUNT = 62;
// Index = vCodex AchievementId.
extern const std::array<AchievementDef, ACHIEVEMENT_COUNT> ACHIEVEMENTS;

struct AchievementState {
  bool unlocked = false;
  uint32_t unlockedAt = 0;  // epoch seconds
};

struct Achievements {
  std::array<AchievementState, ACHIEVEMENT_COUNT> states{};
  // Carried through unchanged so a vCodex file round-trips.
  uint32_t totalBookmarksAdded = 0;
  uint32_t longestSessionMs = 0;
};

struct Progress {
  uint64_t values[8] = {};  // indexed by Metric
  uint64_t of(Metric m) const { return values[static_cast<size_t>(m)]; }
};

Progress computeProgress(const ReadingStats& stats, const Achievements& current, uint64_t dailyGoalMs);

// Unlocks every badge whose target is met; returns the newly unlocked indices.
std::vector<uint8_t> unlock(Achievements& a, const Progress& p, uint32_t epochSeconds);

bool fromJson(const JsonDocument& doc, Achievements& out);
// Writes formatVersion 2, keeping vCodex's other top-level fields from `previous` (may be empty).
void toJson(const Achievements& a, const Progress& p, const JsonDocument& previous, JsonDocument& out);

}  // namespace inkhome::stats
