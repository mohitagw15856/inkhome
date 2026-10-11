#include "ReadingStatsApp.h"

#include <GfxRenderer.h>
#include <HalClock.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>

#include "MappedInputManager.h"
#include "ReadingStatsTracker.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace inkhome::stats {

namespace {

constexpr int MIN_CELL = 10;
constexpr int MAX_CELL = 22;
constexpr int CELL_GAP = 3;
constexpr int MAX_WEEKS = 26;
constexpr uint64_t MINUTE_MS = 60ULL * 1000;

// "2 h 05 m" / "12 m" / "0 m"
void formatDuration(char* out, const size_t size, const uint64_t ms) {
  const uint64_t minutes = ms / MINUTE_MS;
  if (minutes >= 60) {
    snprintf(out, size, "%u h %02u m", static_cast<unsigned>(minutes / 60), static_cast<unsigned>(minutes % 60));
  } else {
    snprintf(out, size, "%u m", static_cast<unsigned>(minutes));
  }
}

uint64_t dayMs(const ReadingStats& s, const uint32_t ordinal) {
  const auto it = std::lower_bound(s.readingDays.begin(), s.readingDays.end(), ordinal,
                                   [](const DayStats& d, uint32_t o) { return d.dayOrdinal < o; });
  return it != s.readingDays.end() && it->dayOrdinal == ordinal ? it->readingMs : 0;
}

}  // namespace

std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<ReadingStatsActivity>(renderer, mappedInput);
}

void ReadingStatsActivity::onEnter() {
  Activity::onEnter();
  ensureLoaded();
  rebuildBookOrder();
  requestUpdate();
}

void ReadingStatsActivity::onExit() {
  bookOrder = {};
  Activity::onExit();
}

void ReadingStatsActivity::rebuildBookOrder() {
  const auto& books = data().books;
  bookOrder.clear();
  bookOrder.reserve(books.size());
  for (size_t i = 0; i < books.size() && i < 0xFFFF; ++i) bookOrder.push_back(static_cast<uint16_t>(i));
  std::sort(bookOrder.begin(), bookOrder.end(), [&books](uint16_t a, uint16_t b) {
    if (books[a].lastReadAt != books[b].lastReadAt) return books[a].lastReadAt > books[b].lastReadAt;
    return books[a].totalReadingMs > books[b].totalReadingMs;
  });
  bookScroll = 0;
}

void ReadingStatsActivity::loop() {
  using Button = MappedInputManager::Button;
  // A result pop-up stays until the next key, which only dismisses it.
  if (notice != Notice::None && notice != Notice::ConfirmImport && mappedInput.wasAnyReleased()) {
    notice = Notice::None;
    requestUpdate();
    return;
  }
  if (mappedInput.wasReleased(Button::Back)) {
    if (notice == Notice::ConfirmImport) {
      notice = Notice::None;
      requestUpdate();
      return;
    }
    finish();
    return;
  }
  if (mappedInput.wasReleased(Button::Confirm)) {
    if (notice == Notice::ConfirmImport) {
      notice = importFrom() ? Notice::Imported : Notice::ImportFailed;
      rebuildBookOrder();
    } else {
      notice = exportTo() ? Notice::Exported : Notice::ExportFailed;
    }
    requestUpdate();
    return;
  }
  if (mappedInput.wasReleased(Button::Left)) {
    notice = Notice::ConfirmImport;
    requestUpdate();
    return;
  }
  const int maxScroll = std::max(0, static_cast<int>(bookOrder.size()) - visibleBooks);
  if ((mappedInput.wasPressed(Button::Down) || mappedInput.wasPressed(Button::Right)) && bookScroll < maxScroll) {
    ++bookScroll;
    requestUpdate();
  } else if ((mappedInput.wasPressed(Button::Up)) && bookScroll > 0) {
    --bookScroll;
    requestUpdate();
  }
}

int ReadingStatsActivity::drawSummary(const int top) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int side = metrics.contentSidePadding;
  const int width = renderer.getScreenWidth() - side * 2;
  const Summary sum = summarise(data(), todayOrdinal());
  const int labelHeight = renderer.getLineHeight(SMALL_FONT_ID);
  const int valueHeight = renderer.getLineHeight(UI_12_FONT_ID);
  const int cardHeight = labelHeight + valueHeight + 12;
  const int columns = width >= 600 ? 4 : 2;  // one row in landscape, two in portrait
  const int cardWidth = (width - (columns - 1) * 8) / columns;

  char values[4][24];
  formatDuration(values[0], sizeof(values[0]), sum.todayMs + pendingMs());
  formatDuration(values[1], sizeof(values[1]), sum.last7Ms + pendingMs());
  snprintf(values[2], sizeof(values[2]), "%u", static_cast<unsigned>(sum.currentStreak));
  formatDuration(values[3], sizeof(values[3]), sum.totalMs + pendingMs());
  const char* labels[4] = {tr(STR_STATS_TODAY), tr(STR_STATS_WEEK), tr(STR_STATS_STREAK), tr(STR_STATS_TOTAL)};

  for (int i = 0; i < 4; ++i) {
    const int x = side + (i % columns) * (cardWidth + 8);
    const int y = top + (i / columns) * (cardHeight + 8);
    renderer.drawRect(x, y, cardWidth, cardHeight);
    renderer.drawText(SMALL_FONT_ID, x + 8, y + 4, labels[i], true);
    renderer.drawText(UI_12_FONT_ID, x + 8, y + 6 + labelHeight, values[i], true, EpdFontFamily::BOLD);
  }
  int y = top + ((4 + columns - 1) / columns) * (cardHeight + 8);

  char line[96];
  snprintf(line, sizeof(line), "%s %u  |  %s %u  |  %s %u", tr(STR_STATS_BOOKS),
           static_cast<unsigned>(sum.booksStarted), tr(STR_STATS_FINISHED), static_cast<unsigned>(sum.booksFinished),
           tr(STR_STATS_LONGEST), static_cast<unsigned>(sum.longestStreak));
  renderer.drawText(SMALL_FONT_ID, side, y, line, true);
  return y + labelHeight + 8;
}

int ReadingStatsActivity::drawHeatmap(const int top) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int side = metrics.contentSidePadding;
  const int width = renderer.getScreenWidth() - side * 2;
  const uint32_t today = todayOrdinal();
  const int labelHeight = renderer.getLineHeight(SMALL_FONT_ID);

  if (today == 0) {
    // The X4 has no real-time clock; the X3's needs setting once.
    const char* hint = halClock.isAvailable() ? tr(STR_STATS_NO_CLOCK) : tr(STR_STATS_NO_RTC);
    const auto lines = renderer.wrappedText(SMALL_FONT_ID, hint, width, 3);
    int y = top;
    for (const auto& l : lines) {
      renderer.drawText(SMALL_FONT_ID, side, y, l.c_str(), true);
      y += labelHeight;
    }
    return y + 8;
  }

  // As many whole weeks as fit at a readable cell size, newest week on the right.
  int cell = MAX_CELL;
  int weeks = (width + CELL_GAP) / (cell + CELL_GAP);
  if (weeks > MAX_WEEKS) weeks = MAX_WEEKS;
  cell = std::clamp((width + CELL_GAP) / weeks - CELL_GAP, MIN_CELL, MAX_CELL);
  const int gridWidth = weeks * (cell + CELL_GAP) - CELL_GAP;
  const int left = side + (width - gridWidth) / 2;

  renderer.drawText(SMALL_FONT_ID, side, top, tr(STR_STATS_HEATMAP), true);
  const int gridTop = top + labelHeight + 4;

  // 1970-01-01 was a Thursday; rows run Monday to Sunday.
  const uint32_t todayRow = (today + 3) % 7;
  const uint32_t firstDay = today - todayRow - static_cast<uint32_t>((weeks - 1) * 7);
  const auto& s = data();
  for (int w = 0; w < weeks; ++w) {
    for (int r = 0; r < 7; ++r) {
      const uint32_t day = firstDay + static_cast<uint32_t>(w * 7 + r);
      if (day > today) continue;
      const int x = left + w * (cell + CELL_GAP);
      const int y = gridTop + r * (cell + CELL_GAP);
      const uint64_t ms = dayMs(s, day) + (day == today ? pendingMs() : 0);
      if (ms == 0) {
        renderer.drawRect(x, y, cell, cell);
      } else if (ms < 15 * MINUTE_MS) {
        renderer.fillRectDither(x, y, cell, cell, LightGray);
        renderer.drawRect(x, y, cell, cell);
      } else if (ms < 45 * MINUTE_MS) {
        renderer.fillRectDither(x, y, cell, cell, DarkGray);
      } else {
        renderer.fillRect(x, y, cell, cell, true);
      }
    }
  }
  return gridTop + 7 * (cell + CELL_GAP) + 8;
}

void ReadingStatsActivity::drawBooks(const int top, const int bottom) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int side = metrics.contentSidePadding;
  const int width = renderer.getScreenWidth() - side * 2;
  const int rowHeight = renderer.getLineHeight(UI_10_FONT_ID) + 6;
  const auto& books = data().books;

  if (books.empty()) {
    renderer.drawText(UI_10_FONT_ID, side, top, tr(STR_STATS_EMPTY), true);
    visibleBooks = 1;
    return;
  }
  renderer.drawLine(side, top, side + width, top);
  visibleBooks = std::max(1, (bottom - top - 6) / rowHeight);
  int y = top + 6;
  char time[24];
  for (int i = bookScroll; i < static_cast<int>(bookOrder.size()) && i < bookScroll + visibleBooks; ++i) {
    const auto& b = books[bookOrder[static_cast<size_t>(i)]];
    formatDuration(time, sizeof(time), b.totalReadingMs);
    const int timeWidth = renderer.getTextWidth(UI_10_FONT_ID, time);
    const std::string& name = b.title.empty() ? b.path : b.title;
    const std::string fitted = renderer.truncatedText(UI_10_FONT_ID, name.c_str(), width - timeWidth - 12);
    renderer.drawText(UI_10_FONT_ID, side, y, fitted.c_str(), true,
                      b.completed ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
    renderer.drawText(UI_10_FONT_ID, side + width - timeWidth, y, time, true);
    y += rowHeight;
  }
}

void ReadingStatsActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_STATS_TITLE));
  int y = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int bottom = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing;

  const LoadState state = ensureLoaded();
  if (state != LoadState::Ready) {
    const StrId message = state == LoadState::TooLarge ? StrId::STR_STATS_TOO_LARGE
                          : state == LoadState::NoCard ? StrId::STR_STATS_NO_CARD
                                                       : StrId::STR_STATS_UNREADABLE;
    for (const auto& l :
         renderer.wrappedText(UI_10_FONT_ID, I18N.get(message), pageWidth - metrics.contentSidePadding * 2, 6)) {
      renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, y, l.c_str(), true);
      y += renderer.getLineHeight(UI_10_FONT_ID);
    }
  } else {
    y = drawSummary(y);
    y = drawHeatmap(y);
    drawBooks(y, bottom);
  }

  if (notice != Notice::None) {
    const StrId message = notice == Notice::Exported       ? StrId::STR_STATS_EXPORTED
                          : notice == Notice::ExportFailed ? StrId::STR_STATS_EXPORT_FAILED
                          : notice == Notice::Imported     ? StrId::STR_STATS_IMPORTED
                          : notice == Notice::ImportFailed ? StrId::STR_STATS_IMPORT_FAILED
                                                           : StrId::STR_STATS_CONFIRM_IMPORT;
    GUI.drawPopup(renderer, I18N.get(message));
  }

  const bool confirming = notice == Notice::ConfirmImport;
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), confirming ? tr(STR_STATS_IMPORT) : tr(STR_STATS_EXPORT),
                                            confirming ? "" : tr(STR_STATS_IMPORT), "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

}  // namespace inkhome::stats
