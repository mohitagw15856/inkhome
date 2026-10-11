#include "YearInBooksApp.h"

#include <GfxRenderer.h>
#include <HalClock.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>
#include <ctime>

#include "MappedInputManager.h"
#include "ReadingStatsTracker.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/ScreenshotUtil.h"

namespace inkhome::stats {

namespace {

#ifndef OMIT_FONTS
constexpr int BIG_FONT_ID = NOTOSANS_18_FONT_ID;
#else
constexpr int BIG_FONT_ID = UI_12_FONT_ID;
#endif
constexpr StrId MONTHS[12] = {StrId::STR_MONTH_1, StrId::STR_MONTH_2,  StrId::STR_MONTH_3,  StrId::STR_MONTH_4,
                              StrId::STR_MONTH_5, StrId::STR_MONTH_6,  StrId::STR_MONTH_7,  StrId::STR_MONTH_8,
                              StrId::STR_MONTH_9, StrId::STR_MONTH_10, StrId::STR_MONTH_11, StrId::STR_MONTH_12};
constexpr StrId WEEKDAYS[7] = {StrId::STR_WEEKDAY_1, StrId::STR_WEEKDAY_2, StrId::STR_WEEKDAY_3, StrId::STR_WEEKDAY_4,
                               StrId::STR_WEEKDAY_5, StrId::STR_WEEKDAY_6, StrId::STR_WEEKDAY_7};
constexpr int PADDING = 24;

int currentYear() {
  struct tm t = {};
  return halClock.localTime(t) && t.tm_year + 1900 >= 2024 ? t.tm_year + 1900 : 0;
}

}  // namespace

std::unique_ptr<Activity> createYear(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<YearInBooksActivity>(renderer, mappedInput);
}

void YearInBooksActivity::onEnter() {
  Activity::onEnter();
  ensureLoaded();
  const int now = currentYear();
  if (now != 0) {
    year = now;
  } else {
    // No clock: the latest year with reading.
    const auto& days = data().readingDays;
    if (!days.empty()) {
      int y;
      unsigned m, d;
      civilFromDays(days.back().dayOrdinal, y, m, d);
      year = y;
    }
  }
  requestUpdate();
}

void YearInBooksActivity::loop() {
  using Button = MappedInputManager::Button;
  if (notice != Notice::None && mappedInput.wasAnyReleased()) {
    notice = Notice::None;
    requestUpdate();
    return;
  }
  if (mappedInput.wasReleased(Button::Back)) {
    finish();
  } else if (mappedInput.wasReleased(Button::Confirm)) {
    pendingSave = Save::ToExports;
    requestUpdate();
  } else if (mappedInput.wasReleased(Button::Left)) {
    pendingSave = Save::ToSleep;
    requestUpdate();
  } else if (mappedInput.wasPressed(Button::Up) || mappedInput.wasPressed(Button::PageBack)) {
    --year;
    requestUpdate();
  } else if (mappedInput.wasPressed(Button::Down) || mappedInput.wasPressed(Button::PageForward)) {
    if (currentYear() == 0 || year < currentYear()) ++year;
    requestUpdate();
  }
}

void YearInBooksActivity::drawCard() {
  const int width = renderer.getScreenWidth();
  // The card stops above the button hints so they never overlap it.
  const int height = renderer.getScreenHeight() - UITheme::getInstance().getMetrics().buttonHintsHeight;
  const YearSummary y = summariseYear(data(), year);
  const int bigH = renderer.getLineHeight(BIG_FONT_ID);
  const int midH = renderer.getLineHeight(UI_12_FONT_ID);
  const int smallH = renderer.getLineHeight(UI_10_FONT_ID);

  renderer.clearScreen();
  renderer.drawRect(PADDING / 2, PADDING / 2, width - PADDING, height - PADDING, 3, true);
  int top = PADDING * 2;
  char buf[96];
  snprintf(buf, sizeof(buf), "%d", year);
  renderer.drawCenteredText(BIG_FONT_ID, top, buf, true, EpdFontFamily::BOLD);
  top += bigH;
  renderer.drawCenteredText(UI_12_FONT_ID, top, tr(STR_YEAR_HEADING), true);
  top += midH + PADDING;

  // Four big figures, two per row.
  char values[4][24];
  snprintf(values[0], sizeof(values[0]), "%u", static_cast<unsigned>(y.totalMs / 3600000));
  snprintf(values[1], sizeof(values[1]), "%u", static_cast<unsigned>(y.daysRead));
  snprintf(values[2], sizeof(values[2]), "%u", static_cast<unsigned>(y.booksFinished));
  snprintf(values[3], sizeof(values[3]), "%u", static_cast<unsigned>(y.longestStreak));
  const StrId labels[4] = {StrId::STR_YEAR_HOURS, StrId::STR_YEAR_DAYS, StrId::STR_YEAR_FINISHED,
                           StrId::STR_YEAR_STREAK};
  const int cellW = (width - PADDING * 3) / 2;
  const int cellH = bigH + smallH + PADDING;
  for (int i = 0; i < 4; ++i) {
    const int x = PADDING + (i % 2) * (cellW + PADDING);
    const int cy = top + (i / 2) * (cellH + PADDING / 2);
    renderer.fillRect(x, cy, cellW, cellH, true);
    const int vw = renderer.getTextWidth(BIG_FONT_ID, values[i], EpdFontFamily::BOLD);
    renderer.drawText(BIG_FONT_ID, x + (cellW - vw) / 2, cy + PADDING / 3, values[i], false, EpdFontFamily::BOLD);
    const char* label = I18N.get(labels[i]);
    const int lw = renderer.getTextWidth(UI_10_FONT_ID, label);
    renderer.drawText(UI_10_FONT_ID, x + (cellW - lw) / 2, cy + PADDING / 3 + bigH, label, false);
  }
  top += 2 * (cellH + PADDING / 2) + PADDING / 2;

  auto row = [&](const char* label, const std::string& value) {
    renderer.drawText(UI_10_FONT_ID, PADDING, top, label, true);
    top += smallH;
    const std::string fitted =
        renderer.truncatedText(UI_12_FONT_ID, value.c_str(), width - PADDING * 2, EpdFontFamily::BOLD);
    renderer.drawText(UI_12_FONT_ID, PADDING, top, fitted.c_str(), true, EpdFontFamily::BOLD);
    top += midH + PADDING / 2;
  };
  if (y.topBook >= 0) {
    const auto& b = data().books[static_cast<size_t>(y.topBook)];
    snprintf(buf, sizeof(buf), "%s (%u h)", b.title.empty() ? b.path.c_str() : b.title.c_str(),
             static_cast<unsigned>(y.topBookMs / 3600000));
    row(tr(STR_YEAR_TOP_BOOK), buf);
  }
  if (y.busiestMonth > 0) row(tr(STR_YEAR_BUSIEST_MONTH), I18N.get(MONTHS[y.busiestMonth - 1]));
  if (y.favouriteWeekday >= 0) row(tr(STR_YEAR_FAVOURITE_DAY), I18N.get(WEEKDAYS[y.favouriteWeekday]));
  if (y.totalMs == 0) renderer.drawCenteredText(UI_12_FONT_ID, top, tr(STR_YEAR_EMPTY), true);

  renderer.drawCenteredText(SMALL_FONT_ID, height - PADDING - renderer.getLineHeight(SMALL_FONT_ID),
                            tr(STR_YEAR_FOOTER), true);
}

void YearInBooksActivity::render(RenderLock&&) {
  drawCard();
  if (pendingSave != Save::None) {
    // Saved before the button hints are drawn, so the picture is just the card.
    char path[64];
    const bool toSleep = pendingSave == Save::ToSleep;
    Storage.mkdir(toSleep ? "/sleep" : "/exports", true);
    snprintf(path, sizeof(path), "%s/year-in-books-%d.bmp", toSleep ? "/sleep" : "/exports", year);
    const bool ok = ScreenshotUtil::saveFramebufferAsBmp(path, renderer.getFrameBuffer(), renderer.getDisplayWidth(),
                                                         renderer.getDisplayHeight());
    notice = !ok ? Notice::Failed : toSleep ? Notice::SavedSleep : Notice::Saved;
    pendingSave = Save::None;
  }
  if (notice != Notice::None) {
    GUI.drawPopup(renderer, notice == Notice::Failed       ? tr(STR_YEAR_SAVE_FAILED)
                            : notice == Notice::SavedSleep ? tr(STR_YEAR_SAVED_SLEEP)
                                                           : tr(STR_YEAR_SAVED));
  }
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_YEAR_SAVE), tr(STR_YEAR_AS_SLEEP), "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

}  // namespace inkhome::stats
