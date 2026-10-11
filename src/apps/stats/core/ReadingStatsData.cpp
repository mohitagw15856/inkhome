#include "ReadingStatsData.h"

#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <numeric>

namespace inkhome::stats {

namespace {

void readDays(JsonArrayConst array, std::vector<DayStats>& out) {
  out.clear();
  out.reserve(array.size());
  for (JsonObjectConst day : array) {
    DayStats d;
    d.dayOrdinal = day["dayOrdinal"] | 0u;
    d.readingMs = day["readingMs"] | static_cast<uint64_t>(0);
    if (d.dayOrdinal != 0 || d.readingMs != 0) out.push_back(d);
  }
}

std::string str(JsonVariantConst v) { return v.is<const char*>() ? std::string(v.as<const char*>()) : std::string(); }

DayStats& dayIn(std::vector<DayStats>& days, const uint32_t ordinal) {
  auto it = std::lower_bound(days.begin(), days.end(), ordinal,
                             [](const DayStats& d, uint32_t o) { return d.dayOrdinal < o; });
  if (it == days.end() || it->dayOrdinal != ordinal) it = days.insert(it, DayStats{ordinal, 0});
  return *it;
}

// --- streaming writer ---

struct Out {
  Sink sink;
  void* ctx;
  void raw(const char* s) const {
    size_t n = 0;
    while (s[n]) ++n;
    sink(ctx, s, n);
  }
  void num(const uint64_t v) const {
    char buf[24];
    const int n = snprintf(buf, sizeof(buf), "%" PRIu64, v);
    sink(ctx, buf, static_cast<size_t>(n));
  }
  void boolean(const bool v) const { raw(v ? "true" : "false"); }
  void string(const std::string& s) const {
    raw("\"");
    size_t start = 0;
    for (size_t i = 0; i < s.size(); ++i) {
      const auto c = static_cast<unsigned char>(s[i]);
      if (c != '"' && c != '\\' && c >= 0x20) continue;
      if (i > start) sink(ctx, s.data() + start, i - start);
      char esc[8];
      if (c == '"' || c == '\\') {
        esc[0] = '\\';
        esc[1] = static_cast<char>(c);
        esc[2] = 0;
      } else {
        snprintf(esc, sizeof(esc), "\\u%04x", c);
      }
      raw(esc);
      start = i + 1;
    }
    if (start < s.size()) sink(ctx, s.data() + start, s.size() - start);
    raw("\"");
  }
  void days(const std::vector<DayStats>& list) const {
    raw("[");
    bool first = true;
    for (const auto& d : list) {
      if (!first) raw(",");
      first = false;
      raw("{\"dayOrdinal\":");
      num(d.dayOrdinal);
      raw(",\"readingMs\":");
      num(d.readingMs);
      raw("}");
    }
    raw("]");
  }
  void field(const char* key) const {
    raw(",\"");
    raw(key);
    raw("\":");
  }
};

}  // namespace

uint32_t daysFromCivil(int y, const unsigned m, const unsigned d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return static_cast<uint32_t>(era * 146097 + static_cast<int>(doe) - 719468);
}

void civilFromDays(const uint32_t days, int& year, unsigned& month, unsigned& day) {
  const int z = static_cast<int>(days) + 719468;
  const int era = (z >= 0 ? z : z - 146096) / 146097;
  const unsigned doe = static_cast<unsigned>(z - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  day = doy - (153 * mp + 2) / 5 + 1;
  month = mp < 10 ? mp + 3 : mp - 9;
  year = static_cast<int>(yoe) + era * 400 + (month <= 2);
}

bool fromJson(const JsonDocument& doc, ReadingStats& out) {
  out = ReadingStats{};
  const uint32_t version = doc["formatVersion"] | 0u;
  if (version < 1 || version > FORMAT_VERSION) return false;
  if (version >= 6 && !(doc["readingDays"].is<JsonArrayConst>() && doc["legacyReadingDays"].is<JsonArrayConst>() &&
                        doc["sessionLog"].is<JsonArrayConst>() && doc["books"].is<JsonArrayConst>())) {
    return false;
  }

  readDays(doc["readingDays"], out.readingDays);
  readDays(doc["legacyReadingDays"], out.legacyReadingDays);

  JsonArrayConst log = doc["sessionLog"];
  out.sessionLog.reserve(std::min<size_t>(log.size(), MAX_SESSION_LOG));
  for (JsonObjectConst e : log) {
    SessionLogEntry s;
    s.dayOrdinal = e["dayOrdinal"] | 0u;
    s.sessionMs = e["sessionMs"] | 0u;
    s.bookId = str(e["bookId"]);
    s.path = str(e["path"]);
    out.sessionLog.push_back(std::move(s));
  }
  if (out.sessionLog.size() > MAX_SESSION_LOG) {
    out.sessionLog.erase(out.sessionLog.begin(), out.sessionLog.end() - MAX_SESSION_LOG);
  }

  JsonArrayConst books = doc["books"];
  out.books.reserve(books.size());
  for (JsonObjectConst b : books) {
    BookStats book;
    book.bookId = str(b["bookId"]);
    book.path = str(b["path"]);
    for (JsonVariantConst p : b["knownPaths"].as<JsonArrayConst>()) {
      if (book.knownPaths.size() < MAX_KNOWN_PATHS && p.is<const char*>())
        book.knownPaths.emplace_back(p.as<const char*>());
    }
    book.title = str(b["title"]);
    book.author = str(b["author"]);
    book.coverBmpPath = str(b["coverBmpPath"]);
    book.chapterTitle = str(b["chapterTitle"]);
    readDays(b["readingDays"], book.readingDays);
    book.totalReadingMs = b["totalReadingMs"] | static_cast<uint64_t>(0);
    book.sessions = b["sessions"] | 0u;
    book.lastSessionMs = b["lastSessionMs"] | 0u;
    book.firstReadAt = b["firstReadAt"] | 0u;
    book.lastReadAt = b["lastReadAt"] | 0u;
    book.completedAt = b["completedAt"] | 0u;
    book.lastProgressPercent = b["lastProgressPercent"] | static_cast<uint8_t>(0);
    book.chapterProgressPercent = b["chapterProgressPercent"] | static_cast<uint8_t>(0);
    book.completed = b["completed"] | false;
    if (book.path.empty() && book.bookId.empty()) continue;
    out.books.push_back(std::move(book));
  }

  std::sort(out.readingDays.begin(), out.readingDays.end(),
            [](const DayStats& a, const DayStats& b) { return a.dayOrdinal < b.dayOrdinal; });
  return true;
}

void toJson(const ReadingStats& stats, const Sink sink, void* ctx) {
  const Out o{sink, ctx};
  o.raw("{\"formatVersion\":");
  o.num(FORMAT_VERSION);
  o.field("readingDays");
  o.days(stats.readingDays);
  o.field("legacyReadingDays");
  o.days(stats.legacyReadingDays);
  o.field("sessionLog");
  o.raw("[");
  bool first = true;
  for (const auto& s : stats.sessionLog) {
    if (!first) o.raw(",");
    first = false;
    o.raw("{\"dayOrdinal\":");
    o.num(s.dayOrdinal);
    o.field("sessionMs");
    o.num(s.sessionMs);
    if (!s.bookId.empty()) {
      o.field("bookId");
      o.string(s.bookId);
    }
    if (!s.path.empty()) {
      o.field("path");
      o.string(s.path);
    }
    o.raw("}");
  }
  o.raw("]");
  o.field("books");
  o.raw("[");
  first = true;
  for (const auto& b : stats.books) {
    if (!first) o.raw(",");
    first = false;
    o.raw("{\"bookId\":");
    o.string(b.bookId);
    o.field("path");
    o.string(b.path);
    o.field("knownPaths");
    o.raw("[");
    for (size_t i = 0; i < b.knownPaths.size(); ++i) {
      if (i) o.raw(",");
      o.string(b.knownPaths[i]);
    }
    o.raw("]");
    o.field("title");
    o.string(b.title);
    o.field("author");
    o.string(b.author);
    o.field("coverBmpPath");
    o.string(b.coverBmpPath);
    o.field("chapterTitle");
    o.string(b.chapterTitle);
    o.field("totalReadingMs");
    o.num(b.totalReadingMs);
    o.field("sessions");
    o.num(b.sessions);
    o.field("lastSessionMs");
    o.num(b.lastSessionMs);
    o.field("firstReadAt");
    o.num(b.firstReadAt);
    o.field("lastReadAt");
    o.num(b.lastReadAt);
    o.field("completedAt");
    o.num(b.completedAt);
    o.field("lastProgressPercent");
    o.num(b.lastProgressPercent);
    o.field("chapterProgressPercent");
    o.num(b.chapterProgressPercent);
    o.field("completed");
    o.boolean(b.completed);
    o.field("readingDays");
    o.days(b.readingDays);
    o.raw("}");
  }
  o.raw("]}");
}

int findBook(const ReadingStats& stats, const std::string& bookId, const std::string& path) {
  if (!bookId.empty()) {
    for (size_t i = 0; i < stats.books.size(); ++i) {
      if (stats.books[i].bookId == bookId) return static_cast<int>(i);
    }
  }
  for (size_t i = 0; i < stats.books.size(); ++i) {
    const auto& b = stats.books[i];
    if (b.path == path) return static_cast<int>(i);
    if (std::find(b.knownPaths.begin(), b.knownPaths.end(), path) != b.knownPaths.end()) return static_cast<int>(i);
  }
  return -1;
}

void recordReading(ReadingStats& stats, const size_t bookIndex, const uint32_t dayOrdinal, const uint64_t ms) {
  if (bookIndex >= stats.books.size() || ms == 0) return;
  auto& book = stats.books[bookIndex];
  book.totalReadingMs += ms;
  if (dayOrdinal == 0) return;
  dayIn(book.readingDays, dayOrdinal).readingMs += ms;
  dayIn(stats.readingDays, dayOrdinal).readingMs += ms;
}

void recordSession(ReadingStats& stats, const size_t bookIndex, const uint32_t dayOrdinal, const uint32_t sessionMs,
                   const uint32_t epochSeconds) {
  if (bookIndex >= stats.books.size()) return;
  auto& book = stats.books[bookIndex];
  book.sessions += 1;
  book.lastSessionMs = sessionMs;
  if (epochSeconds != 0) {
    if (book.firstReadAt == 0) book.firstReadAt = epochSeconds;
    book.lastReadAt = std::max(book.lastReadAt, epochSeconds);
  }
  if (stats.sessionLog.size() >= MAX_SESSION_LOG) {
    stats.sessionLog.erase(stats.sessionLog.begin(), stats.sessionLog.end() - (MAX_SESSION_LOG - 1));
  }
  stats.sessionLog.push_back(SessionLogEntry{dayOrdinal, sessionMs, book.bookId, book.path});
}

Summary summarise(const ReadingStats& stats, const uint32_t today) {
  Summary s;
  for (const auto& b : stats.books) {
    s.totalMs += b.totalReadingMs;
    if (b.completed) ++s.booksFinished;
  }
  for (const auto& d : stats.legacyReadingDays) s.totalMs += d.readingMs;
  s.booksStarted = static_cast<uint32_t>(stats.books.size());

  uint32_t run = 0;
  uint32_t prev = 0;
  for (const auto& d : stats.readingDays) {
    if (d.readingMs == 0) continue;
    run = (prev != 0 && d.dayOrdinal == prev + 1) ? run + 1 : 1;
    prev = d.dayOrdinal;
    s.longestStreak = std::max(s.longestStreak, run);
    if (today != 0 && d.dayOrdinal <= today) {
      if (d.dayOrdinal == today) s.todayMs += d.readingMs;
      if (today - d.dayOrdinal < 7) s.last7Ms += d.readingMs;
      if (today - d.dayOrdinal < 30) s.last30Ms += d.readingMs;
    }
  }
  // Current streak: consecutive days ending today, or yesterday if today has no reading yet.
  if (today != 0 && prev != 0 && (prev == today || prev + 1 == today)) s.currentStreak = run;
  return s;
}

YearSummary summariseYear(const ReadingStats& stats, const int year) {
  YearSummary y;
  y.year = year;
  const uint32_t first = daysFromCivil(year, 1, 1);
  const uint32_t last = daysFromCivil(year, 12, 31);
  uint64_t months[13] = {};
  uint64_t weekdays[7] = {};
  uint32_t run = 0;
  uint32_t prev = 0;
  for (const auto& d : stats.readingDays) {
    if (d.dayOrdinal < first || d.dayOrdinal > last || d.readingMs == 0) continue;
    y.totalMs += d.readingMs;
    ++y.daysRead;
    run = (prev != 0 && d.dayOrdinal == prev + 1) ? run + 1 : 1;
    prev = d.dayOrdinal;
    y.longestStreak = std::max(y.longestStreak, run);
    int yy;
    unsigned mm, dd;
    civilFromDays(d.dayOrdinal, yy, mm, dd);
    months[mm] += d.readingMs;
    weekdays[(d.dayOrdinal + 3) % 7] += d.readingMs;  // 1970-01-01 was a Thursday
  }
  for (int m = 1; m <= 12; ++m) {
    if (months[m] > 0 && (y.busiestMonth == 0 || months[m] > months[y.busiestMonth])) y.busiestMonth = m;
  }
  for (int w = 0; w < 7; ++w) {
    if (weekdays[w] > 0 && (y.favouriteWeekday < 0 || weekdays[w] > weekdays[y.favouriteWeekday])) {
      y.favouriteWeekday = w;
    }
  }
  const uint32_t firstEpoch = first * 86400UL;
  const uint32_t lastEpoch = (last + 1) * 86400UL;
  for (size_t i = 0; i < stats.books.size(); ++i) {
    const auto& b = stats.books[i];
    // Older files may mark a book completed without a date; its last read stands in.
    const uint32_t finishedAt = b.completedAt != 0 ? b.completedAt : b.lastReadAt;
    if (b.completed && finishedAt >= firstEpoch && finishedAt < lastEpoch) ++y.booksFinished;
    const uint64_t ms =
        std::accumulate(b.readingDays.begin(), b.readingDays.end(), static_cast<uint64_t>(0),
                        [first, last](uint64_t sum, const DayStats& d) {
                          return d.dayOrdinal >= first && d.dayOrdinal <= last ? sum + d.readingMs : sum;
                        });
    if (ms > y.topBookMs) {
      y.topBookMs = ms;
      y.topBook = static_cast<int>(i);
    }
  }
  return y;
}

}  // namespace inkhome::stats
