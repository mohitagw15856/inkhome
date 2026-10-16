#include "FocusTimer.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdio>

#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "MappedInputManager.h"
#include "RecentBooksStore.h"
#include "apps/inkwriter/InkWriterApp.h"
#include "apps/inkwriter/WritingStats.h"
#include "apps/stats/ReadingStatsTracker.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace fui = freeink::ui;

// Defined in main.cpp; the break screen is opened from the main loop, outside any activity.
extern GfxRenderer renderer;
extern MappedInputManager mappedInputManager;

namespace inkhome::focus {

namespace {

constexpr uint8_t LENGTHS[] = {15, 25, 45, 60};
bool running = false;
bool breakShown = false;
uint8_t minutes = 25;
unsigned long endMs = 0;

std::string currentBook() {
  if (!APP_STATE.openEpubPath.empty()) return APP_STATE.openEpubPath;
  const auto& recent = RECENT_BOOKS.getBooks();
  return recent.empty() ? std::string() : recent.front().path;
}

}  // namespace

std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<FocusActivity>(renderer, mappedInput);
}

void start(const uint8_t length) {
  minutes = length;
  endMs = millis() + static_cast<unsigned long>(length) * 60UL * 1000UL;
  running = true;
  breakShown = false;
  LOG_INF("FOCUS", "Focus timer: %u min", static_cast<unsigned>(length));
}

void stop() { running = false; }
bool active() { return running; }
uint8_t lengthMinutes() { return minutes; }

// Break reminder: once per reading session, after the chosen minutes of reading.
static bool reminderShown = false;

static void remindIfDue(const bool readerActive) {
  static constexpr uint32_t REMINDER_MINUTES[] = {0, 30, 45, 60};
  const uint32_t minutesSetting = REMINDER_MINUTES[SETTINGS.breakReminder < 4 ? SETTINGS.breakReminder : 0];
  const uint64_t sessionMs = stats::currentSessionMs();
  if (sessionMs == 0) reminderShown = false;  // a new session starts the count again
  if (minutesSetting == 0 || !readerActive || reminderShown || running) return;
  if (sessionMs < static_cast<uint64_t>(minutesSetting) * 60000ULL) return;
  reminderShown = true;
  minutes = static_cast<uint8_t>(minutesSetting);
  if (auto screen = makeUniqueNoThrow<BreakActivity>(::renderer, ::mappedInputManager)) {
    activityManager.pushActivity(std::move(screen));
  }
}

void tick(const bool readerActive) {
  remindIfDue(readerActive);
  if (!running || breakShown) return;
  if (static_cast<long>(millis() - endMs) < 0) return;
  running = false;
  breakShown = true;
  // Only interrupt reading; elsewhere the timer simply ends.
  if (!readerActive) return;
  if (auto screen = makeUniqueNoThrow<BreakActivity>(::renderer, ::mappedInputManager)) {
    activityManager.pushActivity(std::move(screen));
  }
}

// ---------------------------------------------------------------- picker

void FocusActivity::onEnter() {
  UiListActivity::onEnter();
  rowCount = 0;
  for (int i = 0; i < LENGTH_COUNT; ++i) {
    snprintf(labels[i], sizeof(labels[i]), "%u %s", static_cast<unsigned>(LENGTHS[i]), tr(STR_FOCUS_MINUTES));
    rows[rowCount] = fui::ListItem{};
    rows[rowCount].label = labels[i];
    rows[rowCount].actionValue = static_cast<int16_t>(rowCount);
    ++rowCount;
  }
  // Inkhome: a 25-minute writing sprint in InkWriter.
  rows[rowCount] = fui::ListItem{};
  rows[rowCount].label = tr(STR_FOCUS_WRITE);
  rows[rowCount].actionValue = static_cast<int16_t>(rowCount);
  ++rowCount;
  if (running) {
    rows[rowCount] = fui::ListItem{};
    rows[rowCount].label = tr(STR_FOCUS_STOP);
    rows[rowCount].actionValue = static_cast<int16_t>(rowCount);
    ++rowCount;
  }
  nav.reset(1);  // 25 minutes
}

int FocusActivity::listCount() const { return rowCount; }

const char* FocusActivity::headerTitle() const { return tr(STR_FOCUS_TITLE); }

void FocusActivity::activateIndex(const int index) {
  if (index < 0 || index >= rowCount) return;
  app.clearTapFlash();
  if (index == LENGTH_COUNT) {
    inkwriter::setPendingSprint(25);
    auto writer = inkwriter::create(renderer, mappedInput);
    if (writer) activityManager.replaceActivity(std::move(writer));
    return;
  }
  if (index > LENGTH_COUNT) {
    stop();
    finish();
    return;
  }
  start(LENGTHS[index]);
  const std::string book = currentBook();
  if (book.empty()) {
    finish();  // the timer still runs; it shows the break once a book is open
    return;
  }
  activityManager.goToReader(book);
}

void FocusActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMarginFromScreen(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                                static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));
  fui::ListProps props;
  props.items = rows;
  props.count = static_cast<uint16_t>(rowCount);
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  syncListViewport(screen, props);
  screen.list(props);
}

// ---------------------------------------------------------------- break

void BreakActivity::onEnter() {
  Activity::onEnter();
  requestUpdate();
}

void BreakActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    start(minutes);
    finish();
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
  }
}

void BreakActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const int pageHeight = renderer.getScreenHeight();
  const int big = renderer.getLineHeight(UI_12_FONT_ID);
  char line[64];
  snprintf(line, sizeof(line), tr(STR_FOCUS_DONE_FMT), static_cast<unsigned>(minutes));
  renderer.drawCenteredText(UI_12_FONT_ID, pageHeight / 2 - big * 2, tr(STR_FOCUS_BREAK), true, EpdFontFamily::BOLD);
  renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - big / 2, line, true);
  renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + big, tr(STR_FOCUS_STRETCH), true);
  const auto labels = mappedInput.mapLabels(tr(STR_FOCUS_FINISH), tr(STR_FOCUS_AGAIN), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer(HalDisplay::HALF_REFRESH);
}

}  // namespace inkhome::focus
