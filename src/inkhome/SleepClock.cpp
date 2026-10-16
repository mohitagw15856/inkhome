#include "SleepClock.h"

#include <GfxRenderer.h>
#include <HalClock.h>
#include <HalPowerManager.h>
#include <I18n.h>

#include <cstdio>
#include <ctime>

#include "CrossPointSettings.h"
#include "fontIds.h"

namespace inkhome::sleep {

namespace {

// Segments a..g as bits 0..6 (a top, b top right, c bottom right, d bottom,
// e bottom left, f top left, g middle).
constexpr uint8_t DIGIT_SEGMENTS[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};

// Indexed by tm_wday (0 Sunday); the existing strings run Monday (1) to Sunday (7).
constexpr StrId WEEKDAYS[7] = {StrId::STR_WEEKDAY_7, StrId::STR_WEEKDAY_1, StrId::STR_WEEKDAY_2,
                               StrId::STR_WEEKDAY_3, StrId::STR_WEEKDAY_4, StrId::STR_WEEKDAY_5,
                               StrId::STR_WEEKDAY_6};
constexpr StrId MONTHS[12] = {StrId::STR_MONTH_1,  StrId::STR_MONTH_2,  StrId::STR_MONTH_3, StrId::STR_MONTH_4,
                              StrId::STR_MONTH_5,  StrId::STR_MONTH_6,  StrId::STR_MONTH_7, StrId::STR_MONTH_8,
                              StrId::STR_MONTH_9,  StrId::STR_MONTH_10, StrId::STR_MONTH_11, StrId::STR_MONTH_12};

// One digit in a w x h box with stroke t.
void drawDigit(const GfxRenderer& renderer, const int x, const int y, const int w, const int h, const int t,
               const int digit) {
  const uint8_t s = DIGIT_SEGMENTS[digit % 10];
  const int mid = y + (h - t) / 2;
  if (s & 0x01) renderer.fillRect(x + t, y, w - 2 * t, t, true);
  if (s & 0x02) renderer.fillRect(x + w - t, y + t, t, mid - y - t + t / 2, true);
  if (s & 0x04) renderer.fillRect(x + w - t, mid + t / 2, t, y + h - t - mid - t / 2, true);
  if (s & 0x08) renderer.fillRect(x + t, y + h - t, w - 2 * t, t, true);
  if (s & 0x10) renderer.fillRect(x, mid + t / 2, t, y + h - t - mid - t / 2, true);
  if (s & 0x20) renderer.fillRect(x, y + t, t, mid - y - t + t / 2, true);
  if (s & 0x40) renderer.fillRect(x + t, mid, w - 2 * t, t, true);
}

}  // namespace

void drawClock(const GfxRenderer& renderer) {
  renderer.clearScreen();
  const int width = renderer.getScreenWidth();
  const int height = renderer.getScreenHeight();

  struct tm now {};
  if (!halClock.isAvailable() || !halClock.localTime(now) || now.tm_year < 124) {
    renderer.drawCenteredText(UI_12_FONT_ID, height / 2 - 20, tr(STR_CLOCK_NOT_SET), true, EpdFontFamily::BOLD);
    renderer.drawCenteredText(UI_10_FONT_ID, height / 2 + 10, tr(STR_CLOCK_NOT_SET_HINT), true);
    return;
  }

  int hour = now.tm_hour;
  if (SETTINGS.clockFormat == 1) {
    hour %= 12;
    if (hour == 0) hour = 12;
  }
  // Four digits and a colon across 80% of the width.
  const int digitW = width * 8 / 10 / 5;
  const int digitH = digitW * 2;
  const int stroke = digitW / 6;
  const int gap = digitW / 4;
  const int colonW = stroke * 2;
  const int total = 4 * digitW + 3 * gap + colonW + gap;
  int x = (width - total) / 2;
  const int y = height / 2 - digitH / 2 - 30;

  renderer.drawCenteredText(UI_10_FONT_ID, y - 40, tr(STR_CLOCK_ASLEEP_SINCE), true);
  const int digits[4] = {hour / 10, hour % 10, now.tm_min / 10, now.tm_min % 10};
  for (int i = 0; i < 4; ++i) {
    if (!(i == 0 && digits[0] == 0 && SETTINGS.clockFormat == 1)) {
      drawDigit(renderer, x, y, digitW, digitH, stroke, digits[i]);
    }
    x += digitW + gap;
    if (i == 1) {
      renderer.fillRect(x, y + digitH / 3 - stroke / 2, colonW, colonW, true);
      renderer.fillRect(x, y + digitH * 2 / 3 - stroke / 2, colonW, colonW, true);
      x += colonW + gap;
    }
  }

  char date[64];
  snprintf(date, sizeof(date), "%s %d %s", I18N.get(WEEKDAYS[now.tm_wday % 7]), now.tm_mday,
           I18N.get(MONTHS[now.tm_mon % 12]));
#ifndef OMIT_FONTS
  constexpr int DATE_FONT_ID = NOTOSERIF_18_FONT_ID;
#else
  constexpr int DATE_FONT_ID = UI_12_FONT_ID;
#endif
  renderer.drawCenteredText(DATE_FONT_ID, y + digitH + 40, date, true);

  char battery[32];
  snprintf(battery, sizeof(battery), "%s %u%%", tr(STR_CLOCK_BATTERY),
           static_cast<unsigned>(powerManager.getBatteryPercentage()));
  renderer.drawCenteredText(UI_10_FONT_ID, height - 60, battery, true);
}

}  // namespace inkhome::sleep
