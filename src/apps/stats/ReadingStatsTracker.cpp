#include "ReadingStatsTracker.h"

#include <ArduinoJson.h>
#include <HalClock.h>
#include <HalStorage.h>
#include <KOReaderDocumentId.h>
#include <Logging.h>

#include <atomic>
#include <ctime>
#include <string>

#include "CrossPointState.h"
#include "RecentBooksStore.h"
#include "inkhome/SdState.h"

namespace inkhome::stats {

namespace {

constexpr unsigned long IDLE_LIMIT_MS = 5UL * 60 * 1000;  // no input for this long: stop counting
constexpr unsigned long MAX_TICK_GAP_MS = 10UL * 1000;    // longer gaps (blocking work, sleep) are not reading
constexpr unsigned long COMMIT_EVERY_MS = 60UL * 1000;    // fold pending time into the history
constexpr unsigned long SAVE_EVERY_MS = 10UL * 60 * 1000;
constexpr uint32_t MIN_SESSION_MS = 30UL * 1000;  // shorter opens are not counted as sessions
constexpr int MIN_VALID_YEAR = 2024;
// Page-pace average: turns closer than this are skips, further apart are pauses.
constexpr unsigned long MIN_TURN_MS = 2000;
constexpr unsigned long MAX_TURN_MS = 5UL * 60 * 1000;
constexpr int MIN_TURNS_FOR_PACE = 8;
constexpr float PACE_WEIGHT = 0.15f;

constexpr uint8_t FINISHED_PERCENT = 100;
std::atomic<int> lastChapterPagesLeft{-1};
std::atomic<int> pendingPercent{-1};
float paceSeconds = 0;
int paceSamples = 0;
unsigned long lastTurnMs = 0;

ReadingStats stats;
LoadState loadState = LoadState::NotLoaded;
bool dirty = false;
unsigned long lastSaveMs = 0;

struct Session {
  bool active = false;
  int bookIndex = -1;
  std::string path;
  unsigned long lastTickMs = 0;
  unsigned long lastInputMs = 0;
  uint64_t pendingMs = 0;
  uint32_t sessionMs = 0;
} session;

struct HalFileReader {
  HalFile& file;
  int read() { return file.read(); }
  size_t readBytes(char* buf, size_t n) {
    const int got = file.read(buf, n);
    return got > 0 ? static_cast<size_t>(got) : 0;
  }
};

bool clockNow(uint32_t& dayOrdinal, uint32_t& epochSeconds) {
  struct tm t = {};
  if (!halClock.localTime(t) || t.tm_year + 1900 < MIN_VALID_YEAR) {
    dayOrdinal = 0;
    epochSeconds = 0;
    return false;
  }
  dayOrdinal = daysFromCivil(t.tm_year + 1900, static_cast<unsigned>(t.tm_mon + 1), static_cast<unsigned>(t.tm_mday));
  epochSeconds = dayOrdinal * 86400UL + static_cast<uint32_t>(t.tm_hour * 3600 + t.tm_min * 60 + t.tm_sec);
  return true;
}

bool loadFile(const char* path, ReadingStats& out, LoadState& state) {
  HalFile file;
  if (!Storage.openFileForRead("RST", path, file)) {
    state = LoadState::Unreadable;
    return false;
  }
  const size_t size = file.fileSize();
  if (size > MAX_FILE_BYTES) {
    LOG_ERR("RST", "%s is %u bytes (limit %u); tracking paused", path, static_cast<unsigned>(size),
            static_cast<unsigned>(MAX_FILE_BYTES));
    state = LoadState::TooLarge;
    return false;
  }
  JsonDocument doc;
  HalFileReader reader{file};
  const auto err = deserializeJson(doc, reader);
  if (err || !fromJson(doc, out)) {
    LOG_ERR("RST", "Could not parse %s: %s", path, err.c_str());
    state = LoadState::Unreadable;
    return false;
  }
  state = LoadState::Ready;
  return true;
}

bool writeFile(const char* path) {
  char tmp[64];
  snprintf(tmp, sizeof(tmp), "%s.tmp", path);
  if (Storage.exists(tmp)) Storage.remove(tmp);
  {
    HalFile file;
    if (!Storage.openFileForWrite("RST", tmp, file)) return false;
    bool ok = true;
    struct Ctx {
      HalFile* file;
      bool* ok;
    } ctx{&file, &ok};
    toJson(
        stats,
        [](void* c, const char* data, size_t len) {
          auto* x = static_cast<Ctx*>(c);
          if (*x->ok && x->file->write(data, len) != len) *x->ok = false;
        },
        &ctx);
    if (!ok) {
      file.close();
      Storage.remove(tmp);
      return false;
    }
  }
  // Keep the previous file as .bak, as vCodex does, then move the new one in.
  char bak[64];
  snprintf(bak, sizeof(bak), "%s.bak", path);
  if (Storage.exists(path)) {
    if (Storage.exists(bak)) Storage.remove(bak);
    Storage.rename(path, bak);
  }
  return Storage.rename(tmp, path);
}

void save() {
  if (loadState != LoadState::Ready || !dirty) return;
  Storage.mkdir("/.crosspoint", true);
  if (writeFile(STATS_PATH)) {
    dirty = false;
    lastSaveMs = millis();
    LOG_DBG("RST", "Saved reading stats (%u books)", static_cast<unsigned>(stats.books.size()));
  } else {
    LOG_ERR("RST", "Saving reading stats failed");
  }
}

void commitPending() {
  if (session.pendingMs == 0 || session.bookIndex < 0) return;
  uint32_t day = 0;
  uint32_t epoch = 0;
  clockNow(day, epoch);
  recordReading(stats, static_cast<size_t>(session.bookIndex), day, session.pendingMs);
  session.sessionMs += static_cast<uint32_t>(session.pendingMs);
  session.pendingMs = 0;
  dirty = true;
}

int bookFor(const std::string& path) {
  const std::string bookId = KOReaderDocumentId::calculate(path);
  int index = findBook(stats, bookId, path);
  if (index < 0) {
    BookStats book;
    book.bookId = bookId.empty() ? "legacy:" + path : bookId;
    book.path = path;
    for (const auto& recent : RECENT_BOOKS.getBooks()) {
      if (recent.path != path) continue;
      book.title = recent.title;
      book.author = recent.author;
      book.coverBmpPath = recent.coverBmpPath;
      break;
    }
    stats.books.push_back(std::move(book));
    index = static_cast<int>(stats.books.size()) - 1;
    dirty = true;
  } else {
    auto& book = stats.books[static_cast<size_t>(index)];
    if (book.path != path) {
      // Moved or renamed: remember where it was, follow it to the new place.
      if (book.knownPaths.size() < MAX_KNOWN_PATHS) book.knownPaths.push_back(book.path);
      book.path = path;
      dirty = true;
    }
  }
  return index;
}

void beginSession(const std::string& path, const unsigned long now) {
  session = Session{};
  session.path = path;
  session.lastTickMs = now;
  session.lastInputMs = now;
  session.active = true;
  if (ensureLoaded() != LoadState::Ready) return;
  session.bookIndex = bookFor(path);
  LOG_DBG("RST", "Session start: %s", path.c_str());
}

void endSession() {
  if (!session.active) return;
  commitPending();
  if (session.bookIndex >= 0 && session.sessionMs >= MIN_SESSION_MS) {
    uint32_t day = 0;
    uint32_t epoch = 0;
    clockNow(day, epoch);
    recordSession(stats, static_cast<size_t>(session.bookIndex), day, session.sessionMs, epoch);
    dirty = true;
  }
  LOG_DBG("RST", "Session end: %u ms", static_cast<unsigned>(session.sessionMs));
  session = Session{};
  save();
}

}  // namespace

LoadState ensureLoaded() {
  if (loadState == LoadState::Ready || loadState == LoadState::TooLarge) return loadState;
  if (!sdMounted()) return LoadState::NoCard;
  if (!Storage.exists(STATS_PATH)) {
    stats = ReadingStats{};
    loadState = LoadState::Ready;
    return loadState;
  }
  ReadingStats loaded;
  if (loadFile(STATS_PATH, loaded, loadState)) stats = std::move(loaded);
  return loadState;
}

const ReadingStats& data() { return stats; }

uint32_t todayOrdinal() {
  uint32_t day = 0;
  uint32_t epoch = 0;
  clockNow(day, epoch);
  return day;
}

uint64_t pendingMs() { return session.pendingMs; }

float secondsPerPage() { return paceSamples >= MIN_TURNS_FOR_PACE ? paceSeconds : 0.0f; }

int chapterPagesLeft() { return lastChapterPagesLeft.load(); }

// Runs on the render task: only hands values over; tick() applies them.
void noteProgress(const float bookPercent, const int chapterPage, const int chapterPages) {
  lastChapterPagesLeft.store(chapterPages >= chapterPage ? chapterPages - chapterPage : -1);
  const int percent = bookPercent < 0 ? 0 : bookPercent > 100 ? 100 : static_cast<int>(bookPercent + 0.5f);
  pendingPercent.store(percent);
}

void applyPendingProgress() {
  const int pending = pendingPercent.exchange(-1);
  if (pending < 0 || !session.active || session.bookIndex < 0) return;
  auto& book = stats.books[static_cast<size_t>(session.bookIndex)];
  const auto percent = static_cast<uint8_t>(pending);
  if (percent != book.lastProgressPercent) {
    book.lastProgressPercent = percent;
    dirty = true;
  }
  if (percent >= FINISHED_PERCENT && !book.completed) {
    book.completed = true;
    uint32_t day = 0;
    uint32_t epoch = 0;
    clockNow(day, epoch);
    book.completedAt = epoch;
    dirty = true;
  }
}

void tick(const bool readerActive, const bool userActivity) {
  const unsigned long now = millis();
  const std::string& path = APP_STATE.openEpubPath;
  const bool reading = readerActive && !path.empty() && sdMounted();

  if (!reading) {
    if (session.active) endSession();
    return;
  }
  if (!session.active || session.path != path) {
    if (session.active) endSession();
    beginSession(path, now);
    return;
  }

  applyPendingProgress();
  if (userActivity) {
    session.lastInputMs = now;
    const unsigned long turnGap = now - lastTurnMs;
    if (lastTurnMs != 0 && turnGap >= MIN_TURN_MS && turnGap <= MAX_TURN_MS) {
      const float seconds = static_cast<float>(turnGap) / 1000.0f;
      paceSeconds = paceSamples == 0 ? seconds : paceSeconds + PACE_WEIGHT * (seconds - paceSeconds);
      ++paceSamples;
    }
    lastTurnMs = now;
  }
  const unsigned long gap = now - session.lastTickMs;
  session.lastTickMs = now;
  if (gap < MAX_TICK_GAP_MS && now - session.lastInputMs <= IDLE_LIMIT_MS) session.pendingMs += gap;

  if (session.pendingMs >= COMMIT_EVERY_MS) commitPending();
  if (dirty && now - lastSaveMs >= SAVE_EVERY_MS) save();
}

void flush() {
  if (session.active) {
    endSession();
  } else {
    save();
  }
}

bool exportTo(const char* path) {
  if (ensureLoaded() != LoadState::Ready) return false;
  commitPending();
  Storage.mkdir("/exports", true);
  return writeFile(path);
}

bool importFrom(const char* path) {
  if (!sdMounted() || !Storage.exists(path)) return false;
  ReadingStats imported;
  LoadState state = LoadState::NotLoaded;
  if (!loadFile(path, imported, state)) return false;
  if (session.active) endSession();
  stats = std::move(imported);
  loadState = LoadState::Ready;
  dirty = true;
  save();
  return true;
}

}  // namespace inkhome::stats
