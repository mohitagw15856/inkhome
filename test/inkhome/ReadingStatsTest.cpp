#include <gtest/gtest.h>

#include <string>

#include "apps/stats/core/ReadingStatsData.h"

using namespace inkhome::stats;

namespace {

// Shaped like a CPR-vCodex formatVersion 6 export.
const char* VCODEX_SAMPLE = R"({"formatVersion":6,
 "readingDays":[{"dayOrdinal":20370,"readingMs":600000},{"dayOrdinal":20371,"readingMs":1200000}],
 "legacyReadingDays":[{"dayOrdinal":20000,"readingMs":5000}],
 "sessionLog":[{"dayOrdinal":20371,"sessionMs":1200000,"bookId":"abc123","path":"/books/a.epub"}],
 "books":[{"bookId":"abc123","path":"/books/a.epub","knownPaths":["/old/a.epub"],"title":"A \"Quoted\" Title",
   "author":"Ann","coverBmpPath":"","chapterTitle":"One","totalReadingMs":1800000,"sessions":2,
   "lastSessionMs":1200000,"firstReadAt":1760000000,"lastReadAt":1760090000,"completedAt":0,
   "lastProgressPercent":42,"chapterProgressPercent":10,"completed":false,
   "readingDays":[{"dayOrdinal":20370,"readingMs":600000},{"dayOrdinal":20371,"readingMs":1200000}]}]})";

std::string serialise(const ReadingStats& s) {
  std::string out;
  toJson(s, [](void* ctx, const char* d, size_t n) { static_cast<std::string*>(ctx)->append(d, n); }, &out);
  return out;
}

bool parse(const std::string& json, ReadingStats& out) {
  JsonDocument doc;
  if (deserializeJson(doc, json)) return false;
  return fromJson(doc, out);
}

}  // namespace

TEST(ReadingStats, LoadsVcodexFormat6) {
  ReadingStats s;
  ASSERT_TRUE(parse(VCODEX_SAMPLE, s));
  ASSERT_EQ(s.books.size(), 1u);
  EXPECT_EQ(s.books[0].title, "A \"Quoted\" Title");
  EXPECT_EQ(s.books[0].lastProgressPercent, 42);
  EXPECT_EQ(s.books[0].knownPaths.size(), 1u);
  EXPECT_EQ(s.readingDays.size(), 2u);
  EXPECT_EQ(s.legacyReadingDays.size(), 1u);
  EXPECT_EQ(s.sessionLog.size(), 1u);
}

TEST(ReadingStats, RoundTripIsLossless) {
  ReadingStats a;
  ASSERT_TRUE(parse(VCODEX_SAMPLE, a));
  const std::string json = serialise(a);
  ReadingStats b;
  ASSERT_TRUE(parse(json, b));
  EXPECT_EQ(serialise(b), json);
  EXPECT_EQ(b.books[0].firstReadAt, 1760000000u);
  EXPECT_EQ(b.books[0].title, "A \"Quoted\" Title");
}

TEST(ReadingStats, RejectsFutureVersionAndIncompleteV6) {
  ReadingStats s;
  EXPECT_FALSE(parse(R"({"formatVersion":7,"readingDays":[],"legacyReadingDays":[],"sessionLog":[],"books":[]})", s));
  EXPECT_FALSE(parse(R"({"formatVersion":6,"readingDays":[]})", s));
  EXPECT_TRUE(parse(R"({"formatVersion":6,"readingDays":[],"legacyReadingDays":[],"sessionLog":[],"books":[]})", s));
  EXPECT_TRUE(parse(R"({"formatVersion":3,"books":[{"path":"/x.epub","totalReadingMs":5}]})", s));
  EXPECT_EQ(s.books[0].totalReadingMs, 5u);
}

TEST(ReadingStats, RecordsTimeAndSessions) {
  ReadingStats s;
  s.books.push_back(BookStats{});
  s.books[0].path = "/b.epub";
  recordReading(s, 0, 20400, 60000);
  recordReading(s, 0, 20400, 30000);
  recordReading(s, 0, 0, 1000);  // unknown clock: book total only
  recordSession(s, 0, 20400, 91000, 1762560000);
  EXPECT_EQ(s.books[0].totalReadingMs, 91000u);
  ASSERT_EQ(s.readingDays.size(), 1u);
  EXPECT_EQ(s.readingDays[0].readingMs, 90000u);
  EXPECT_EQ(s.books[0].sessions, 1u);
  EXPECT_EQ(s.books[0].firstReadAt, 1762560000u);
  EXPECT_EQ(s.sessionLog.back().path, "/b.epub");
}

TEST(ReadingStats, SessionLogIsCapped) {
  ReadingStats s;
  s.books.push_back(BookStats{});
  for (int i = 0; i < 300; ++i) recordSession(s, 0, 20400, 1000, 0);
  EXPECT_EQ(s.sessionLog.size(), MAX_SESSION_LOG);
}

TEST(ReadingStats, SummaryStreaksAndWindows) {
  ReadingStats s;
  s.books.push_back(BookStats{});
  for (uint32_t d : {100u, 101u, 102u, 110u, 111u}) recordReading(s, 0, d, 60000);
  const Summary today = summarise(s, 111);
  EXPECT_EQ(today.currentStreak, 2u);
  EXPECT_EQ(today.longestStreak, 3u);
  EXPECT_EQ(today.todayMs, 60000u);
  EXPECT_EQ(today.last7Ms, 120000u);
  EXPECT_EQ(summarise(s, 112).currentStreak, 2u);  // yesterday still counts
  EXPECT_EQ(summarise(s, 113).currentStreak, 0u);
}

TEST(ReadingStats, CivilDatesRoundTrip) {
  EXPECT_EQ(daysFromCivil(1970, 1, 1), 0u);
  EXPECT_EQ(daysFromCivil(2026, 10, 11), 20737u);
  int y;
  unsigned m, d;
  civilFromDays(20737, y, m, d);
  EXPECT_EQ(y, 2026);
  EXPECT_EQ(m, 10u);
  EXPECT_EQ(d, 11u);
}

TEST(ReadingStats, FindsBookByIdThenPathThenKnownPath) {
  ReadingStats s;
  ASSERT_TRUE(parse(VCODEX_SAMPLE, s));
  EXPECT_EQ(findBook(s, "abc123", "/elsewhere.epub"), 0);
  EXPECT_EQ(findBook(s, "", "/books/a.epub"), 0);
  EXPECT_EQ(findBook(s, "zzz", "/old/a.epub"), 0);
  EXPECT_EQ(findBook(s, "zzz", "/nope.epub"), -1);
}

TEST(ReadingStats, YearSummaryCountsOnlyThatYear) {
  ReadingStats s;
  s.books.resize(2);
  const uint32_t jan5 = daysFromCivil(2026, 1, 5);  // a Monday
  recordReading(s, 0, jan5, 60000);
  recordReading(s, 0, jan5 + 1, 60000);
  recordReading(s, 1, jan5 + 2, 300000);
  recordReading(s, 1, daysFromCivil(2025, 12, 31), 999000);  // previous year
  s.books[1].completed = true;
  s.books[1].completedAt = (jan5 + 2) * 86400UL;
  const YearSummary y = summariseYear(s, 2026);
  EXPECT_EQ(y.totalMs, 420000u);
  EXPECT_EQ(y.daysRead, 3u);
  EXPECT_EQ(y.longestStreak, 3u);
  EXPECT_EQ(y.booksFinished, 1u);
  EXPECT_EQ(y.topBook, 1);
  EXPECT_EQ(y.busiestMonth, 1);
  EXPECT_EQ(y.favouriteWeekday, 2);  // Wednesday held the 5-minute day
}

TEST(ReadingStats, YearCountsUndatedFinishedBooksByLastRead) {
  ReadingStats s;
  s.books.resize(1);
  s.books[0].completed = true;
  s.books[0].lastReadAt = daysFromCivil(2026, 3, 1) * 86400UL;
  EXPECT_EQ(summariseYear(s, 2026).booksFinished, 1u);
  EXPECT_EQ(summariseYear(s, 2025).booksFinished, 0u);
}
