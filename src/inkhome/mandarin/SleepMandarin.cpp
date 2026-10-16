#include "SleepMandarin.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>

#include <cstdio>
#include <cstdlib>

#include "HanziData.h"
#include "fontIds.h"
#include "inkhome/SdState.h"

namespace inkhome::mandarin {

namespace {

constexpr char STATE_DIR[] = "/.crosspoint/apps/mandarin";
constexpr char STATE_PATH[] = "/.crosspoint/apps/mandarin/next.txt";
constexpr int WORD_COUNT = static_cast<int>(sizeof(WORDS) / sizeof(WORDS[0]));
constexpr int SCALE = 2;  // 48 px glyphs drawn at 96 px

// Next word index, advanced and saved for the following sleep.
int takeNextIndex() {
  int index = 0;
  if (sdMounted() && Storage.exists(STATE_PATH)) index = atoi(Storage.readFile(STATE_PATH).c_str());
  if (index < 0 || index >= WORD_COUNT) index = 0;
  if (sdMounted()) {
    Storage.mkdir(STATE_DIR, true);
    char next[12];
    snprintf(next, sizeof(next), "%d", (index + 1) % WORD_COUNT);
    Storage.writeFile(STATE_PATH, String(next));
  }
  return index;
}

void drawGlyph(const GfxRenderer& renderer, const uint16_t glyph, const int x, const int y) {
  if (glyph >= GLYPH_COUNT) return;
  constexpr int rowBytes = GLYPH_SIZE / 8;
  for (int gy = 0; gy < GLYPH_SIZE; ++gy) {
    for (int gx = 0; gx < GLYPH_SIZE; ++gx) {
      if (GLYPHS[glyph][gy * rowBytes + gx / 8] & (0x80 >> (gx & 7))) {
        renderer.fillRect(x + gx * SCALE, y + gy * SCALE, SCALE, SCALE, true);
      }
    }
  }
}

}  // namespace

void drawSleepWord(const GfxRenderer& renderer) {
  renderer.clearScreen();
  const int width = renderer.getScreenWidth();
  const int height = renderer.getScreenHeight();
  const int index = takeNextIndex();
  const Word& word = WORDS[index];

  int count = 0;
  while (count < 4 && word.glyphs[count] != 0xFFFF) ++count;
  const int cell = GLYPH_SIZE * SCALE;
  int x = (width - count * cell) / 2;
  const int y = height / 2 - cell - 20;
  for (int i = 0; i < count; ++i, x += cell) drawGlyph(renderer, word.glyphs[i], x, y);

#ifndef OMIT_FONTS
  constexpr int PINYIN_FONT_ID = NOTOSERIF_18_FONT_ID;
#else
  constexpr int PINYIN_FONT_ID = UI_12_FONT_ID;
#endif
  renderer.drawCenteredText(PINYIN_FONT_ID, y + cell + 30, word.pinyin, true);
  renderer.drawCenteredText(UI_12_FONT_ID, y + cell + 80, word.english, true);

  char caption[48];
  snprintf(caption, sizeof(caption), "HSK 1, %s %d / %d", tr(STR_MANDARIN_WORD), index + 1, WORD_COUNT);
  renderer.drawCenteredText(UI_10_FONT_ID, height - 60, caption, true);
}

}  // namespace inkhome::mandarin
