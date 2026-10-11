#pragma once

// Reading statistics data model, compatible with CPR-vCodex's
// /.crosspoint/reading_stats.json (formatVersion 6), so vCodex users can carry
// their history over and back. Pure C++ plus ArduinoJson; host-tested.
// Schema from CPR-vCodex (franssjz/cpr-vcodex, ReadingStatsStore and
// JsonSettingsIO::saveReadingStats), MIT.

#include <ArduinoJson.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace inkhome::stats {

constexpr uint32_t FORMAT_VERSION = 6;
constexpr size_t MAX_SESSION_LOG = 128;  // vCodex ReadingSessionLog::MAX_ENTRIES
constexpr size_t MAX_KNOWN_PATHS = 8;

struct DayStats {
  uint32_t dayOrdinal = 0;  // days since 1970-01-01, local time
  uint64_t readingMs = 0;
};

struct SessionLogEntry {
  uint32_t dayOrdinal = 0;
  uint32_t sessionMs = 0;
  std::string bookId;
  std::string path;
};

struct BookStats {
  std::string bookId;
  std::string path;
  std::vector<std::string> knownPaths;
  std::string title;
  std::string author;
  std::string coverBmpPath;
  std::string chapterTitle;
  std::vector<DayStats> readingDays;
  uint64_t totalReadingMs = 0;
  uint32_t sessions = 0;
  uint32_t lastSessionMs = 0;
  uint32_t firstReadAt = 0;  // epoch seconds
  uint32_t lastReadAt = 0;
  uint32_t completedAt = 0;
  uint8_t lastProgressPercent = 0;
  uint8_t chapterProgressPercent = 0;
  bool completed = false;
};

struct ReadingStats {
  std::vector<DayStats> readingDays;        // all books, aggregated per day
  std::vector<DayStats> legacyReadingDays;  // time not attributed to a book (older vCodex data)
  std::vector<SessionLogEntry> sessionLog;
  std::vector<BookStats> books;
};

struct Summary {
  uint64_t totalMs = 0;
  uint64_t todayMs = 0;
  uint64_t last7Ms = 0;
  uint64_t last30Ms = 0;
  uint32_t currentStreak = 0;
  uint32_t longestStreak = 0;
  uint32_t booksStarted = 0;
  uint32_t booksFinished = 0;
};

// Civil date to days since 1970-01-01 (Howard Hinnant's algorithm, as vCodex).
uint32_t daysFromCivil(int year, unsigned month, unsigned day);
void civilFromDays(uint32_t days, int& year, unsigned& month, unsigned& day);

// Loads a parsed document. Accepts formatVersion 1..6; for 6 all four arrays
// must be present, as vCodex requires. False leaves `out` empty.
bool fromJson(const JsonDocument& doc, ReadingStats& out);

// Streams formatVersion 6 JSON through `sink` (called many times with
// fragments) so the whole file never sits in RAM.
using Sink = void (*)(void* ctx, const char* data, size_t len);
void toJson(const ReadingStats& stats, Sink sink, void* ctx);

// Index of the book matching `bookId` (or `path` when bookId is empty or
// unmatched), or -1.
int findBook(const ReadingStats& stats, const std::string& bookId, const std::string& path);

// Adds reading time to a book and the day aggregate. `dayOrdinal` 0 means the
// clock is unknown: the time still counts towards the book total.
void recordReading(ReadingStats& stats, size_t bookIndex, uint32_t dayOrdinal, uint64_t ms);

// Closes a session: session counters, the session log (capped) and timestamps.
void recordSession(ReadingStats& stats, size_t bookIndex, uint32_t dayOrdinal, uint32_t sessionMs,
                   uint32_t epochSeconds);

Summary summarise(const ReadingStats& stats, uint32_t todayOrdinal);

}  // namespace inkhome::stats
