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
}  // namespace

void drawSleepCard(const GfxRenderer& renderer) {
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
  const int height = PADDING * 2 + bigHeight + (book ? smallHeight + 4 : 0);
  const int x = (pageWidth - width) / 2;
  const int y = pageHeight - height - BOTTOM_MARGIN;

  renderer.fillRect(x - BORDER, y - BORDER, width + BORDER * 2, height + BORDER * 2, true);
  renderer.fillRect(x, y, width, height, false);
  const int textWidth = width - PADDING * 2;
  const std::string first = renderer.truncatedText(UI_12_FONT_ID, line1, textWidth, EpdFontFamily::BOLD);
  renderer.drawText(UI_12_FONT_ID, x + PADDING, y + PADDING, first.c_str(), true, EpdFontFamily::BOLD);
  if (book) {
    char line2[160];
    snprintf(line2, sizeof(line2), "%s  (%u h %02u m)", book->title.empty() ? book->path.c_str() : book->title.c_str(),
             static_cast<unsigned>(book->totalReadingMs / 3600000),
             static_cast<unsigned>(book->totalReadingMs / 60000 % 60));
    const std::string second = renderer.truncatedText(SMALL_FONT_ID, line2, textWidth);
    renderer.drawText(SMALL_FONT_ID, x + PADDING, y + PADDING + bigHeight + 4, second.c_str(), true);
  }
}

}  // namespace inkhome::stats
