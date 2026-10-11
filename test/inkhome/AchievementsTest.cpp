#include <gtest/gtest.h>

#include "apps/stats/core/Achievements.h"

using namespace inkhome::stats;

TEST(Achievements, TableMatchesVcodexOrder) {
  EXPECT_EQ(ACHIEVEMENTS.size(), 62u);
  EXPECT_EQ(ACHIEVEMENTS[11].metric, Metric::BooksFinished);  // FirstBookFinished
  EXPECT_EQ(ACHIEVEMENTS[19].target, 24ULL * 3600 * 1000);    // ReadingOneDay
  EXPECT_EQ(ACHIEVEMENTS[42].target, 120ULL * 60 * 1000);     // TwoHourSession
  EXPECT_EQ(ACHIEVEMENTS[43].target, 2u);                     // TwoBooksFinished
  EXPECT_EQ(ACHIEVEMENTS[61].target, 95u);                    // NinetyFiveBooksFinished
}

TEST(Achievements, UnlocksFromReadingStats) {
  ReadingStats s;
  s.books.resize(2);
  s.books[0].completed = true;
  s.books[0].sessions = 3;
  s.books[0].lastSessionMs = 20 * 60 * 1000;
  for (uint32_t d = 100; d < 103; ++d) recordReading(s, 0, d, 31 * 60 * 1000);
  Achievements a;
  const Progress p = computeProgress(s, a, 30 * 60 * 1000);
  EXPECT_EQ(p.of(Metric::GoalDays), 3u);
  EXPECT_EQ(p.of(Metric::MaxGoalStreak), 3u);
  const auto fresh = unlock(a, p, 1760000000);
  EXPECT_TRUE(a.states[0].unlocked);    // first book started
  EXPECT_TRUE(a.states[11].unlocked);   // first book finished
  EXPECT_TRUE(a.states[28].unlocked);   // three-day goal streak
  EXPECT_TRUE(a.states[37].unlocked);   // fifteen-minute session
  EXPECT_FALSE(a.states[38].unlocked);  // thirty-minute session
  EXPECT_EQ(a.states[0].unlockedAt, 1760000000u);
  EXPECT_TRUE(unlock(a, p, 1770000000).empty());  // nothing twice
  EXPECT_FALSE(fresh.empty());
}

TEST(Achievements, JsonRoundTripKeepsVcodexFields) {
  JsonDocument prev;
  deserializeJson(prev, R"({"formatVersion":2,"currentGoalStreak":4,"startedBooks":["/a.epub"],
    "totalBookmarksAdded":12,"states":[{"unlocked":true,"unlockedAt":5}]})");
  Achievements a;
  ASSERT_TRUE(fromJson(prev, a));
  EXPECT_TRUE(a.states[0].unlocked);
  EXPECT_EQ(a.totalBookmarksAdded, 12u);
  JsonDocument out;
  toJson(a, Progress{}, prev, out);
  EXPECT_EQ(out["currentGoalStreak"].as<int>(), 4);
  EXPECT_EQ(out["startedBooks"][0].as<std::string>(), "/a.epub");
  EXPECT_EQ(out["states"].size(), 62u);
  Achievements back;
  ASSERT_TRUE(fromJson(out, back));
  EXPECT_EQ(back.states[0].unlockedAt, 5u);
}
