#include "SleepStatsCard.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <string>

#include "CrossPointState.h"
#include "ReadingStatsTracker.h"
#include "fontIds.h"

namespace inkhome::stats {

namespace {
constexpr int MAX_CARD_WIDTH = 440;
constexpr int PADDING = 14;
constexpr int BORDER = 2;
constexpr int BOTTOM_MARGIN = 40;
constexpr int BAR_HEIGHT = 8;
}  // namespace

void drawSleepCard(const GfxRenderer& renderer, const int bottomReserve) {
  if (ensureLoaded() != LoadState::Ready) return;
  const auto& s = data();
  const uint32_t today = todayOrdinal();
  const Summary sum = summarise(s, today);

  // The book being read, if any.
  const BookStats* book = nullptr;
  const int index = findBook(s, "", APP_STATE.openEpubPath);
  if (APP_STATE.lastSleepFromReader && index >= 0) book = &s.books[static_cast<size_t>(index)];

  const uint64_t todayMs = sum.todayMs + pendingMs();
  char line1[64];
  if (today != 0) {
    snprintf(line1, sizeof(line1), "%s %u m  |  %s %u", tr(STR_STATS_TODAY), static_cast<unsigned>(todayMs / 60000),
             tr(STR_STATS_STREAK), static_cast<unsigned>(sum.currentStreak));
  } else {
    snprintf(line1, sizeof(line1), "%s %u h", tr(STR_STATS_TOTAL), static_cast<unsigned>(sum.totalMs / 3600000));
  }

  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int width = std::min(pageWidth - 2 * 24, MAX_CARD_WIDTH);
  const int bigHeight = renderer.getLineHeight(UI_12_FONT_ID);
  const int smallHeight = renderer.getLineHeight(SMALL_FONT_ID);
  // Book rows: title line, then a progress bar with "42% read  ~6 min left".
  const int bookRows = book ? (smallHeight + 4) * 2 + BAR_HEIGHT + 4 : 0;
  const int height = PADDING * 2 + bigHeight + bookRows;
  const int x = (pageWidth - width) / 2;
  const int y = pageHeight - height - BOTTOM_MARGIN - bottomReserve;

  renderer.fillRect(x - BORDER, y - BORDER, width + BORDER * 2, height + BORDER * 2, true);
  renderer.fillRect(x, y, width, height, false);
  const int textWidth = width - PADDING * 2;
  const std::string first = renderer.truncatedText(UI_12_FONT_ID, line1, textWidth, EpdFontFamily::BOLD);
  renderer.drawText(UI_12_FONT_ID, x + PADDING, y + PADDING, first.c_str(), true, EpdFontFamily::BOLD);
  if (book) {
    // The open session's time is not folded in yet when the sleep screen draws.
    const uint64_t bookMs = book->totalReadingMs + pendingMs();
    const auto bookMinutes = static_cast<unsigned>(bookMs / 60000);
    const char* name = book->title.empty() ? book->path.c_str() : book->title.c_str();
    char line2[160];
    if (bookMinutes >= 60) {
      snprintf(line2, sizeof(line2), "%s  (%u h %02u m)", name, bookMinutes / 60, bookMinutes % 60);
    } else {
      snprintf(line2, sizeof(line2), "%s  (%u min)", name, bookMinutes < 1 ? 1U : bookMinutes);
    }
    const std::string second = renderer.truncatedText(SMALL_FONT_ID, line2, textWidth);
    int rowY = y + PADDING + bigHeight + 4;
    renderer.drawText(SMALL_FONT_ID, x + PADDING, rowY, second.c_str(), true);
    rowY += smallHeight + 4;

    const int percent = book->lastProgressPercent;
    renderer.drawRect(x + PADDING, rowY, textWidth, BAR_HEIGHT);
    renderer.fillRect(x + PADDING, rowY, textWidth * percent / 100, BAR_HEIGHT, true);
    rowY += BAR_HEIGHT + 4;

    char line3[64];
    int used = snprintf(line3, sizeof(line3), tr(STR_STATS_PROGRESS_FMT), static_cast<unsigned>(percent));
    const float pace = secondsPerPage();
    const int pagesLeft = chapterPagesLeft();
    if (pace > 0 && pagesLeft >= 0 && used > 0 && used < static_cast<int>(sizeof(line3))) {
      const int minutes = static_cast<int>(pagesLeft * pace / 60.0f + 0.5f);
      snprintf(line3 + used, sizeof(line3) - used, "  |  ~%d %s", minutes < 1 ? 1 : minutes, tr(STR_STATS_MIN_LEFT));
    }
    renderer.drawText(SMALL_FONT_ID, x + PADDING, rowY, line3, true);
  }
}

}  // namespace inkhome::stats
