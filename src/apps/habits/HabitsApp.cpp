#include "HabitsApp.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>

#include "MappedInputManager.h"
#include "apps/stats/ReadingStatsTracker.h"
#include "components/UITheme.h"
#include "core/habitcore/CompletionLog.h"
#include "core/habitcore/HabitConfig.h"
#include "core/habitcore/Stats.h"
#include "fontIds.h"
#include "inkhome/SdState.h"

namespace inkhome::habits {

namespace {

constexpr char DIR[] = "/.habitink";
constexpr char LOG_DIR[] = "/.habitink/logs";
constexpr char CONFIG_PATH[] = "/.habitink/habits.tsv";
constexpr int HISTORY_DAYS = 400;  // the firmware's year window plus margin
constexpr size_t MAX_FILE_BYTES = 64 * 1024;

std::string logPath(const int id) {
  char path[40];
  snprintf(path, sizeof(path), "%s/%02d.log", LOG_DIR, id);
  return path;
}

std::string readSmall(const char* path) {
  HalFile file;
  if (!Storage.openFileForRead("HAB", path, file) || file.fileSize() > MAX_FILE_BYTES) return {};
  std::string text(file.fileSize(), '\0');
  if (file.read(text.data(), text.size()) != static_cast<int>(text.size())) return {};
  return text;
}

// Habitink's first-run habits, so the screen is never empty.
void seedStarterHabits(habitcore::HabitConfig& config) {
  struct Seed {
    const char* icon;
    const char* schedule;
    const char* name;
  };
  static constexpr Seed SEEDS[] = {
      {"water", "daily", "Drink 2L water"},
      {"run", "1111100", "Morning run"},
      {"book", "0000011", "Weekend reading"},
  };
  for (const Seed& seed : SEEDS) {
    habitcore::Habit h;
    h.icon = seed.icon;
    h.name = seed.name;
    habitcore::Schedule::parseField(seed.schedule, h.schedule);
    config.add(h);
  }
}

bool loadConfig(habitcore::HabitConfig& config) {
  if (Storage.exists(CONFIG_PATH)) return habitcore::HabitConfig::parse(readSmall(CONFIG_PATH), config);
  seedStarterHabits(config);
  Storage.mkdir(LOG_DIR, true);
  Storage.writeFile(CONFIG_PATH, String(config.serialise().c_str()));
  return true;
}

void loadLog(const int id, const habitcore::Date& today, habitcore::CompletionLog& log) {
  log.setWindow(today.toDayNumber() - HISTORY_DAYS, today.toDayNumber());
  const std::string text = readSmall(logPath(id).c_str());
  size_t pos = 0;
  while (pos < text.size()) {
    size_t end = text.find('\n', pos);
    if (end == std::string::npos) end = text.size();
    log.applyLine(text.substr(pos, end - pos));
    pos = end + 1;
  }
}

bool todayDate(habitcore::Date& out) {
  const uint32_t day = stats::todayOrdinal();
  if (day == 0) return false;
  out = habitcore::Date::fromDayNumber(static_cast<int32_t>(day));
  return true;
}

}  // namespace

std::vector<TodayItem> loadToday() {
  std::vector<TodayItem> items;
  habitcore::Date today;
  if (!sdMounted() || !todayDate(today)) return items;
  habitcore::HabitConfig config;
  if (!loadConfig(config)) return items;
  items.reserve(config.habits().size());
  for (const auto& h : config.habits()) {
    habitcore::CompletionLog log;
    loadLog(h.id, today, log);
    const auto streak = habitcore::computeStreak(h.schedule, log, today);
    items.push_back({h.id, h.name, streak.dueToday, streak.doneToday, streak.current});
  }
  // Due habits first, in file order.
  std::stable_sort(items.begin(), items.end(), [](const TodayItem& a, const TodayItem& b) { return a.due > b.due; });
  return items;
}

std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<HabitsActivity>(renderer, mappedInput);
}

void HabitsActivity::onEnter() {
  Activity::onEnter();
  habitcore::Date today;
  noClock = !todayDate(today);
  items = loadToday();
  selected = 0;
  requestUpdate();
}

void HabitsActivity::onExit() {
  items = {};
  Activity::onExit();
}

void HabitsActivity::toggle(const size_t index) {
  habitcore::Date today;
  if (index >= items.size() || !todayDate(today)) return;
  auto& item = items[index];
  item.done = !item.done;
  // Habitink's logs are append-only; the last record for a day wins.
  const std::string path = logPath(item.id);
  std::string text = Storage.exists(path.c_str()) ? readSmall(path.c_str()) : std::string();
  if (text.empty()) text = std::string(habitcore::CompletionLog::headerLine()) + "\n";
  text += habitcore::CompletionLog::formatRecord(today, item.done) + "\n";
  Storage.mkdir(LOG_DIR, true);
  Storage.writeFile(path.c_str(), String(text.c_str()));
  // Streak changes with the tick; recompute just this habit.
  habitcore::HabitConfig config;
  if (loadConfig(config)) {
    if (const auto* h = config.byId(item.id)) {
      habitcore::CompletionLog log;
      loadLog(item.id, today, log);
      item.streak = habitcore::computeStreak(h->schedule, log, today).current;
    }
  }
}

void HabitsActivity::loop() {
  using Button = MappedInputManager::Button;
  if (mappedInput.wasReleased(Button::Back)) {
    finish();
    return;
  }
  const int count = static_cast<int>(items.size());
  if (count == 0) return;
  if (mappedInput.wasPressed(Button::Down) || mappedInput.wasPressed(Button::Right)) {
    selected = (selected + 1) % count;
    requestUpdate();
  } else if (mappedInput.wasPressed(Button::Up) || mappedInput.wasPressed(Button::Left)) {
    selected = (selected + count - 1) % count;
    requestUpdate();
  } else if (mappedInput.wasReleased(Button::Confirm)) {
    toggle(static_cast<size_t>(selected));
    requestUpdate();
  }
}

void HabitsActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int width = renderer.getScreenWidth();
  const int side = metrics.contentSidePadding;
  const int rowH = renderer.getLineHeight(UI_12_FONT_ID) + renderer.getLineHeight(SMALL_FONT_ID) + 14;
  const int box = renderer.getLineHeight(UI_12_FONT_ID);

  int done = 0;
  int due = 0;
  for (const auto& i : items) {
    if (i.due) ++due;
    if (i.due && i.done) ++done;
  }
  char subtitle[32] = {};
  if (!items.empty()) snprintf(subtitle, sizeof(subtitle), "%d / %d", done, due);
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, width, metrics.headerHeight}, tr(STR_HABITS_TITLE), subtitle);
  int y = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;

  if (noClock || items.empty()) {
    const char* message = noClock ? tr(STR_HABITS_NO_CLOCK) : tr(STR_HABITS_NO_CARD);
    for (const auto& l : renderer.wrappedText(UI_12_FONT_ID, message, width - side * 2, 6)) {
      renderer.drawText(UI_12_FONT_ID, side, y, l.c_str(), true);
      y += renderer.getLineHeight(UI_12_FONT_ID);
    }
  } else {
    const int bottom = renderer.getScreenHeight() - metrics.buttonHintsHeight - metrics.verticalSpacing;
    const int visible = std::max(1, (bottom - y) / rowH);
    const int top = std::clamp(selected - visible + 1, 0, std::max(0, static_cast<int>(items.size()) - visible));
    char line[64];
    for (int i = top; i < static_cast<int>(items.size()) && i < top + visible; ++i) {
      const auto& item = items[static_cast<size_t>(i)];
      const int rowY = y + (i - top) * rowH;
      if (i == selected) renderer.fillRectDither(side - 6, rowY - 4, width - (side - 6) * 2, rowH - 4, LightGray);
      renderer.drawRect(side, rowY, box, box, 2, true);
      if (item.done) renderer.fillRect(side + 5, rowY + 5, box - 10, box - 10, true);
      const std::string name = renderer.truncatedText(UI_12_FONT_ID, item.name.c_str(), width - side * 3 - box,
                                                      item.due ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
      renderer.drawText(UI_12_FONT_ID, side + box + 12, rowY, name.c_str(), true,
                        item.due ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
      if (item.due) {
        snprintf(line, sizeof(line), tr(STR_HABITS_STREAK_FMT), item.streak);
      } else {
        snprintf(line, sizeof(line), "%s", tr(STR_HABITS_REST_DAY));
      }
      renderer.drawText(SMALL_FONT_ID, side + box + 12, rowY + renderer.getLineHeight(UI_12_FONT_ID), line, true);
    }
  }

  const auto labels =
      mappedInput.mapLabels(tr(STR_BACK), items.empty() ? "" : tr(STR_HABITS_TICK), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

}  // namespace inkhome::habits
