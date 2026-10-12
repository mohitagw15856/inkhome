#include "AchievementsApp.h"

#include <ArduinoJson.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>

#include "CrossPointSettings.h"
#include "ReadingStatsTracker.h"
#include "components/UITheme.h"
#include "core/Achievements.h"
#include "inkhome/SdState.h"

namespace fui = freeink::ui;

namespace inkhome::stats {

namespace {

constexpr char PATH[] = "/.crosspoint/achievements.json";
constexpr size_t MAX_FILE_BYTES = 32 * 1024;
constexpr uint64_t GOAL_MINUTES[] = {15, 30, 45, 60};  // vCodex dailyGoalTarget order

uint64_t dailyGoalMs() {
  const uint8_t i = SETTINGS.dailyGoalTarget < 4 ? SETTINGS.dailyGoalTarget : 1;
  return GOAL_MINUTES[i] * 60ULL * 1000ULL;
}

bool loadDoc(JsonDocument& doc) {
  if (!sdMounted() || !Storage.exists(PATH)) return false;
  HalFile file;
  if (!Storage.openFileForRead("ACH", PATH, file) || file.fileSize() > MAX_FILE_BYTES) return false;
  std::string text(file.fileSize(), '\0');
  if (file.read(text.data(), text.size()) != static_cast<int>(text.size())) return false;
  return !deserializeJson(doc, text);
}

bool saveDoc(const JsonDocument& doc) {
  std::string text;
  serializeJson(doc, text);
  char tmp[64];
  snprintf(tmp, sizeof(tmp), "%s.tmp", PATH);
  if (!Storage.writeFile(tmp, String(text.c_str()))) return false;
  if (Storage.exists(PATH)) Storage.remove(PATH);
  return Storage.rename(tmp, PATH);
}

void titleFor(const AchievementDef& def, char* out, size_t size) {
  const auto target = static_cast<unsigned>(def.target);
  // First-time badges read naturally ("Finish your first book", not "Finish 1 books").
  if (def.target == 1 || (def.metric == Metric::TotalReadingMs && def.target == 3600000ULL)) {
    StrId first = StrId::STR_ACH_FIRST_BOOK;
    switch (def.metric) {
      case Metric::BooksFinished:
        first = StrId::STR_ACH_FIRST_FINISH;
        break;
      case Metric::Sessions:
        first = StrId::STR_ACH_FIRST_SESSION;
        break;
      case Metric::TotalReadingMs:
        first = StrId::STR_ACH_FIRST_HOUR;
        break;
      case Metric::GoalDays:
        first = StrId::STR_ACH_FIRST_GOAL;
        break;
      case Metric::Bookmarks:
        first = StrId::STR_ACH_FIRST_BOOKMARK;
        break;
      default:
        break;
    }
    snprintf(out, size, "%s", I18N.get(first));
    return;
  }
  switch (def.metric) {
    case Metric::BooksStarted:
      snprintf(out, size, tr(STR_ACH_BOOKS_STARTED), target);
      break;
    case Metric::BooksFinished:
      snprintf(out, size, tr(STR_ACH_BOOKS_FINISHED), target);
      break;
    case Metric::Sessions:
      snprintf(out, size, tr(STR_ACH_SESSIONS), target);
      break;
    case Metric::TotalReadingMs:
      snprintf(out, size, tr(STR_ACH_HOURS), static_cast<unsigned>(def.target / 3600000ULL));
      break;
    case Metric::GoalDays:
      snprintf(out, size, tr(STR_ACH_GOAL_DAYS), target);
      break;
    case Metric::MaxGoalStreak:
      snprintf(out, size, tr(STR_ACH_GOAL_STREAK), target);
      break;
    case Metric::Bookmarks:
      snprintf(out, size, tr(STR_ACH_BOOKMARKS), target);
      break;
    case Metric::MaxSessionMs:
      snprintf(out, size, tr(STR_ACH_SESSION_LENGTH), static_cast<unsigned>(def.target / 60000ULL));
      break;
  }
}

uint64_t shown(const AchievementDef& def, const uint64_t value) {
  if (def.metric == Metric::TotalReadingMs) return value / 3600000ULL;
  if (def.metric == Metric::MaxSessionMs) return value / 60000ULL;
  return value;
}

}  // namespace

std::unique_ptr<Activity> createAchievements(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<AchievementsActivity>(renderer, mappedInput);
}

int refreshAchievements() {
  if (ensureLoaded() != LoadState::Ready) return 0;
  JsonDocument prev;
  Achievements a;
  if (loadDoc(prev)) fromJson(prev, a);
  const Progress p = computeProgress(data(), a, dailyGoalMs());
  const uint32_t epoch = todayOrdinal() * 86400UL;
  const auto fresh = unlock(a, p, epoch);
  if (fresh.empty() && !prev.isNull()) return 0;
  JsonDocument out;
  toJson(a, p, prev, out);
  Storage.mkdir("/.crosspoint", true);
  if (!saveDoc(out)) LOG_ERR("ACH", "Saving achievements failed");
  if (!fresh.empty()) LOG_INF("ACH", "%u new achievements", static_cast<unsigned>(fresh.size()));
  return static_cast<int>(fresh.size());
}

void AchievementsActivity::onEnter() {
  UiListActivity::onEnter();
  refreshAchievements();
  ensureLoaded();
  JsonDocument doc;
  Achievements a;
  if (loadDoc(doc)) fromJson(doc, a);
  const Progress p = computeProgress(data(), a, dailyGoalMs());

  std::vector<uint8_t> order(ACHIEVEMENT_COUNT);
  for (size_t i = 0; i < ACHIEVEMENT_COUNT; ++i) order[i] = static_cast<uint8_t>(i);
  auto ratio = [&](uint8_t i) {
    return static_cast<double>(p.of(ACHIEVEMENTS[i].metric)) / static_cast<double>(ACHIEVEMENTS[i].target);
  };
  std::stable_sort(order.begin(), order.end(), [&](uint8_t x, uint8_t y) {
    const auto& sx = a.states[x];
    const auto& sy = a.states[y];
    if (sx.unlocked != sy.unlocked) return sx.unlocked;
    if (sx.unlocked) return sx.unlockedAt > sy.unlockedAt;
    return ratio(x) > ratio(y);
  });

  const int unlocked = static_cast<int>(
      std::count_if(a.states.begin(), a.states.end(), [](const AchievementState& s) { return s.unlocked; }));
  char head[48];
  snprintf(head, sizeof(head), tr(STR_ACH_HEADER), unlocked, static_cast<int>(ACHIEVEMENT_COUNT));
  header = head;

  labels.assign(ACHIEVEMENT_COUNT, {});
  subtitles.assign(ACHIEVEMENT_COUNT, {});
  rows.assign(ACHIEVEMENT_COUNT, fui::ListItem{});
  char buf[96];
  for (size_t k = 0; k < ACHIEVEMENT_COUNT; ++k) {
    const uint8_t i = order[k];
    const auto& def = ACHIEVEMENTS[i];
    titleFor(def, buf, sizeof(buf));
    labels[k] = buf;
    if (a.states[i].unlocked) {
      int y = 0;
      unsigned m = 0, d = 0;
      civilFromDays(a.states[i].unlockedAt / 86400UL, y, m, d);
      if (a.states[i].unlockedAt >= 86400UL * 365) {
        snprintf(buf, sizeof(buf), tr(STR_ACH_UNLOCKED_ON), d, m, y);
      } else {
        snprintf(buf, sizeof(buf), "%s", tr(STR_ACH_UNLOCKED));
      }
    } else {
      snprintf(buf, sizeof(buf), "%u / %u",
               static_cast<unsigned>(std::min(shown(def, p.of(def.metric)), shown(def, def.target))),
               static_cast<unsigned>(shown(def, def.target)));
    }
    subtitles[k] = buf;
  }
  for (size_t k = 0; k < ACHIEVEMENT_COUNT; ++k) {
    rows[k].label = labels[k].c_str();
    rows[k].subtitle = subtitles[k].c_str();
    rows[k].actionValue = static_cast<int16_t>(k);
    rows[k].enabled = a.states[order[k]].unlocked;
  }
}

void AchievementsActivity::onExit() {
  rows = {};
  labels = {};
  subtitles = {};
  UiListActivity::onExit();
}

int AchievementsActivity::listCount() const { return static_cast<int>(rows.size()); }

void AchievementsActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMarginFromScreen(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                                static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));
  fui::ListProps props;
  props.items = rows.data();
  props.count = static_cast<uint16_t>(rows.size());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  syncListViewport(screen, props);
  screen.list(props);
}

}  // namespace inkhome::stats
