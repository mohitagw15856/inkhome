#include <gtest/gtest.h>

#include "apps/habits/core/habitcore/CompletionLog.h"
#include "apps/habits/core/habitcore/HabitConfig.h"
#include "apps/habits/core/habitcore/Stats.h"

using namespace habitcore;

// The example from Habitink's docs/FORMAT.md, CRLF and a comment included.
constexpr char EXAMPLE[] =
    "# habitink habits v1\r\n# id\ticon\tschedule\tname\r\n1\twater\tdaily\tDrink 2L water\r\n"
    "2\trun\t1111100\tMorning run\n\n3\tbook\t0000011\tWeekend reading\n";

TEST(Habits, ParsesHabitinkExampleWithoutStringStreams) {
  HabitConfig config;
  ASSERT_TRUE(HabitConfig::parse(EXAMPLE, config));
  ASSERT_EQ(config.size(), 3);
  EXPECT_EQ(config.habits()[0].name, "Drink 2L water");
  EXPECT_FALSE(config.habits()[1].schedule.isDaily());
}

TEST(Habits, SerialiseRoundTrips) {
  HabitConfig a;
  ASSERT_TRUE(HabitConfig::parse(EXAMPLE, a));
  HabitConfig b;
  ASSERT_TRUE(HabitConfig::parse(a.serialise(), b));
  ASSERT_EQ(b.size(), 3);
  EXPECT_EQ(b.habits()[2].schedule.mask(), a.habits()[2].schedule.mask());
  EXPECT_EQ(a.serialise().rfind("# habitink habits v1\n", 0), 0u);
}

TEST(Habits, StreakFromLog) {
  CompletionLog log;
  const Date today(2026, 10, 12);
  for (int i = 0; i < 4; ++i) log.applyRecord(Date::fromDayNumber(today.toDayNumber() - i), true);
  const auto s = computeStreak(Schedule::daily(), log, today);
  EXPECT_EQ(s.current, 4);
  EXPECT_TRUE(s.doneToday);
}
