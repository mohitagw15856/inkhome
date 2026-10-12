#include "Achievements.h"

#include <algorithm>

namespace inkhome::stats {

namespace {
constexpr uint64_t HOUR_MS = 60ULL * 60 * 1000;
constexpr uint64_t MINUTE_MS = 60ULL * 1000;
}  // namespace

// Order and targets match vCodex's AchievementId enum and kDefinitions.
const std::array<AchievementDef, ACHIEVEMENT_COUNT> ACHIEVEMENTS = {{
    {Metric::BooksStarted, 1},                // FirstBookStarted
    {Metric::BooksStarted, 5},                // FiveBooksStarted
    {Metric::BooksStarted, 10},               // TenBooksStarted
    {Metric::BooksStarted, 25},               // TwentyFiveBooksStarted
    {Metric::BooksStarted, 50},               // FiftyBooksStarted
    {Metric::Sessions, 1},                    // FirstSession
    {Metric::Sessions, 10},                   // TenSessions
    {Metric::Sessions, 25},                   // TwentyFiveSessions
    {Metric::Sessions, 50},                   // FiftySessions
    {Metric::Sessions, 100},                  // OneHundredSessions
    {Metric::Sessions, 200},                  // TwoHundredSessions
    {Metric::BooksFinished, 1},               // FirstBookFinished
    {Metric::BooksFinished, 3},               // ThreeBooksFinished
    {Metric::BooksFinished, 5},               // FiveBooksFinished
    {Metric::BooksFinished, 10},              // TenBooksFinished
    {Metric::BooksFinished, 20},              // TwentyBooksFinished
    {Metric::TotalReadingMs, 1 * HOUR_MS},    // ReadingOneHour
    {Metric::TotalReadingMs, 5 * HOUR_MS},    // ReadingFiveHours
    {Metric::TotalReadingMs, 10 * HOUR_MS},   // ReadingTenHours
    {Metric::TotalReadingMs, 24 * HOUR_MS},   // ReadingOneDay
    {Metric::TotalReadingMs, 50 * HOUR_MS},   // ReadingFiftyHours
    {Metric::TotalReadingMs, 100 * HOUR_MS},  // ReadingOneHundredHours
    {Metric::TotalReadingMs, 200 * HOUR_MS},  // ReadingTwoHundredHours
    {Metric::GoalDays, 1},                    // FirstGoalDay
    {Metric::GoalDays, 7},                    // SevenGoalDays
    {Metric::GoalDays, 30},                   // ThirtyGoalDays
    {Metric::GoalDays, 60},                   // SixtyGoalDays
    {Metric::GoalDays, 80},                   // EightyGoalDays
    {Metric::MaxGoalStreak, 3},               // ThreeGoalStreak
    {Metric::MaxGoalStreak, 7},               // SevenGoalStreak
    {Metric::MaxGoalStreak, 14},              // FourteenGoalStreak
    {Metric::MaxGoalStreak, 30},              // ThirtyGoalStreak
    {Metric::MaxGoalStreak, 60},              // SixtyGoalStreak
    {Metric::Bookmarks, 1},                   // FirstBookmark
    {Metric::Bookmarks, 10},                  // TenBookmarks
    {Metric::Bookmarks, 25},                  // TwentyFiveBookmarks
    {Metric::Bookmarks, 50},                  // FiftyBookmarks
    {Metric::MaxSessionMs, 15 * MINUTE_MS},   // FifteenMinuteSession
    {Metric::MaxSessionMs, 30 * MINUTE_MS},   // ThirtyMinuteSession
    {Metric::MaxSessionMs, 45 * MINUTE_MS},   // FortyFiveMinuteSession
    {Metric::MaxSessionMs, 60 * MINUTE_MS},   // SixtyMinuteSession
    {Metric::MaxSessionMs, 90 * MINUTE_MS},   // NinetyMinuteSession
    {Metric::MaxSessionMs, 120 * MINUTE_MS},  // TwoHourSession
    {Metric::BooksFinished, 2},               // TwoBooksFinished
    {Metric::BooksFinished, 7},               // SevenBooksFinished
    {Metric::BooksFinished, 15},              // FifteenBooksFinished
    {Metric::BooksFinished, 25},              // TwentyFiveBooksFinished
    {Metric::BooksFinished, 30},              // ThirtyBooksFinished
    {Metric::BooksFinished, 40},              // FortyBooksFinished
    {Metric::BooksFinished, 50},              // FiftyBooksFinished
    {Metric::BooksFinished, 60},              // SixtyBooksFinished
    {Metric::BooksFinished, 75},              // SeventyFiveBooksFinished
    {Metric::BooksFinished, 100},             // OneHundredBooksFinished
    {Metric::BooksFinished, 35},              // ThirtyFiveBooksFinished
    {Metric::BooksFinished, 45},              // FortyFiveBooksFinished
    {Metric::BooksFinished, 55},              // FiftyFiveBooksFinished
    {Metric::BooksFinished, 65},              // SixtyFiveBooksFinished
    {Metric::BooksFinished, 70},              // SeventyBooksFinished
    {Metric::BooksFinished, 80},              // EightyBooksFinished
    {Metric::BooksFinished, 85},              // EightyFiveBooksFinished
    {Metric::BooksFinished, 90},              // NinetyBooksFinished
    {Metric::BooksFinished, 95},              // NinetyFiveBooksFinished
}};

Progress computeProgress(const ReadingStats& stats, const Achievements& current, const uint64_t dailyGoalMs) {
  Progress p;
  auto set = [&p](Metric m, uint64_t v) { p.values[static_cast<size_t>(m)] = v; };
  uint64_t finished = 0;
  uint64_t sessions = 0;
  uint64_t longest = current.longestSessionMs;
  for (const auto& b : stats.books) {
    if (b.completed) ++finished;
    sessions += b.sessions;
    longest = std::max<uint64_t>(longest, b.lastSessionMs);
  }
  for (const auto& e : stats.sessionLog) longest = std::max<uint64_t>(longest, e.sessionMs);

  uint64_t goalDays = 0;
  uint64_t run = 0;
  uint64_t best = 0;
  uint32_t prev = 0;
  for (const auto& d : stats.readingDays) {
    if (d.readingMs < dailyGoalMs) continue;
    ++goalDays;
    run = (prev != 0 && d.dayOrdinal == prev + 1) ? run + 1 : 1;
    prev = d.dayOrdinal;
    best = std::max(best, run);
  }

  set(Metric::BooksStarted, stats.books.size());
  set(Metric::BooksFinished, finished);
  set(Metric::Sessions, sessions);
  set(Metric::TotalReadingMs, summarise(stats, 0).totalMs);
  set(Metric::GoalDays, goalDays);
  set(Metric::MaxGoalStreak, best);
  set(Metric::Bookmarks, current.totalBookmarksAdded);
  set(Metric::MaxSessionMs, longest);
  return p;
}

std::vector<uint8_t> unlock(Achievements& a, const Progress& p, const uint32_t epochSeconds) {
  std::vector<uint8_t> fresh;
  for (size_t i = 0; i < ACHIEVEMENT_COUNT; ++i) {
    auto& s = a.states[i];
    if (s.unlocked || p.of(ACHIEVEMENTS[i].metric) < ACHIEVEMENTS[i].target) continue;
    s.unlocked = true;
    s.unlockedAt = epochSeconds;
    fresh.push_back(static_cast<uint8_t>(i));
  }
  return fresh;
}

bool fromJson(const JsonDocument& doc, Achievements& out) {
  out = Achievements{};
  const uint32_t version = doc["formatVersion"] | 0u;
  if (version < 1 || version > 2) return false;
  JsonArrayConst states = doc["states"];
  size_t i = 0;
  for (JsonObjectConst s : states) {
    if (i >= ACHIEVEMENT_COUNT) break;
    out.states[i].unlocked = s["unlocked"] | false;
    out.states[i].unlockedAt = s["unlockedAt"] | 0u;
    ++i;
  }
  out.totalBookmarksAdded = doc["totalBookmarksAdded"] | 0u;
  out.longestSessionMs = doc["longestSessionMs"] | 0u;
  return true;
}

void toJson(const Achievements& a, const Progress& p, const JsonDocument& previous, JsonDocument& out) {
  out.set(previous);  // keeps vCodex's other counters (goal streak bookkeeping, started/finished lists)
  out["formatVersion"] = 2;
  out["accumulatedReadingMs"] = p.of(Metric::TotalReadingMs);
  out["countedSessions"] = p.of(Metric::Sessions);
  out["totalBookmarksAdded"] = a.totalBookmarksAdded;
  out["longestSessionMs"] = std::max<uint64_t>(a.longestSessionMs, p.of(Metric::MaxSessionMs));
  out["goalDaysCount"] = p.of(Metric::GoalDays);
  out["maxGoalStreak"] = p.of(Metric::MaxGoalStreak);
  JsonArray states = out["states"].to<JsonArray>();
  for (const auto& s : a.states) {
    JsonObject o = states.add<JsonObject>();
    o["unlocked"] = s.unlocked;
    o["unlockedAt"] = s.unlockedAt;
  }
  if (!out["startedBooks"].is<JsonArray>()) out["startedBooks"].to<JsonArray>();
  if (!out["finishedBooks"].is<JsonArray>()) out["finishedBooks"].to<JsonArray>();
}

}  // namespace inkhome::stats
