#include "WritingStats.h"

#include <HalStorage.h>

#include "apps/stats/ReadingStatsTracker.h"
#include "core/WordLog.h"
#include "inkhome/SdState.h"

namespace inkwriter {

namespace {

constexpr char STATE_DIR[] = "/.crosspoint/apps/inkwriter";
constexpr char WORDS_PATH[] = "/.crosspoint/apps/inkwriter/words.txt";
uint8_t pendingSprint = 0;

WordLog load() {
  if (!inkhome::sdMounted() || !Storage.exists(WORDS_PATH)) return {};
  return WordLog::parse(Storage.readFile(WORDS_PATH).c_str());
}

}  // namespace

void logWords(const uint32_t words) {
  const uint32_t today = inkhome::stats::todayOrdinal();
  if (words == 0 || today == 0 || !inkhome::sdMounted()) return;
  WordLog log = load();
  log.add(today, words);
  Storage.mkdir(STATE_DIR, true);
  Storage.writeFile(WORDS_PATH, String(log.serialise().c_str()));
}

uint64_t wordsWritten(const uint32_t firstDay, const uint32_t lastDay) { return load().total(firstDay, lastDay); }

void setPendingSprint(const uint8_t minutes) { pendingSprint = minutes; }

uint8_t takePendingSprint() {
  const uint8_t m = pendingSprint;
  pendingSprint = 0;
  return m;
}

}  // namespace inkwriter
