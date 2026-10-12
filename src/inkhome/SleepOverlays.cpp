#include "SleepOverlays.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <string>

#include "CrossPointSettings.h"
#include "apps/habits/HabitsApp.h"
#include "apps/iffound/OwnerNote.h"
#include "apps/stats/SleepStatsCard.h"
#include "fontIds.h"

namespace inkhome::sleep {

namespace {

constexpr int PADDING = 10;
constexpr int MAX_LINES = 2;

// "If found: <first line>" plus the next non-empty line, in a white band
// with a rule under it, across the top of the screen.
void drawOwnerStrip(const GfxRenderer& renderer) {
  const std::string note = iffound::ownerNote();
  std::string lines[MAX_LINES];
  int count = 0;
  size_t pos = 0;
  while (pos < note.size() && count < MAX_LINES) {
    size_t end = note.find('\n', pos);
    if (end == std::string::npos) end = note.size();
    std::string line = note.substr(pos, end - pos);
    pos = end + 1;
    if (line.find_first_not_of(" \t") == std::string::npos) continue;
    lines[count++] = std::move(line);
  }
  if (count == 0) return;

  const int width = renderer.getScreenWidth();
  const int lineHeight = renderer.getLineHeight(SMALL_FONT_ID);
  const int height = PADDING * 2 + lineHeight * count;
  renderer.fillRect(0, 0, width, height, false);
  renderer.drawLine(0, height, width, height);
  std::string first = std::string(tr(STR_OWNER_STRIP_PREFIX)) + " " + lines[0];
  for (int i = 0; i < count; ++i) {
    const std::string text =
        renderer.truncatedText(SMALL_FONT_ID, (i == 0 ? first : lines[i]).c_str(), width - PADDING * 2,
                               i == 0 ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
    renderer.drawCenteredText(SMALL_FONT_ID, PADDING + i * lineHeight, text.c_str(), true,
                              i == 0 ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
  }
}

int habitsStripHeight(const GfxRenderer& renderer) { return PADDING * 2 + renderer.getLineHeight(SMALL_FONT_ID); }

// Today's due habits as tick boxes in a white band along the bottom edge.
void drawHabitsStrip(const GfxRenderer& renderer) {
  const auto items = habits::loadToday();
  if (items.empty()) return;
  const int width = renderer.getScreenWidth();
  const int lineHeight = renderer.getLineHeight(SMALL_FONT_ID);
  const int box = lineHeight - 6;
  const int height = habitsStripHeight(renderer);
  const int top = renderer.getScreenHeight() - height;
  renderer.fillRect(0, top, width, height, false);
  renderer.drawLine(0, top, width, top);
  int x = PADDING;
  for (const auto& item : items) {
    if (!item.due) continue;
    const int nameWidth = renderer.getTextWidth(SMALL_FONT_ID, item.name.c_str());
    if (x + box + 6 + nameWidth > width - PADDING) break;
    const int boxY = top + PADDING + 3;
    renderer.drawRect(x, boxY, box, box);
    if (item.done) renderer.fillRect(x + 3, boxY + 3, box - 6, box - 6, true);
    renderer.drawText(SMALL_FONT_ID, x + box + 6, top + PADDING, item.name.c_str(), true);
    x += box + 6 + nameWidth + PADDING * 2;
  }
}

}  // namespace

Overlays current(const bool customStatsMode) {
  Overlays o;
  o.statsCard = customStatsMode;
  o.ownerStrip = SETTINGS.sleepOwnerStrip != 0;
  o.habits = SETTINGS.sleepHabits != 0;
  return o;
}

void draw(const GfxRenderer& renderer, const Overlays& overlays) {
  if (overlays.statsCard) stats::drawSleepCard(renderer, overlays.habits ? habitsStripHeight(renderer) : 0);
  if (overlays.ownerStrip) drawOwnerStrip(renderer);
  if (overlays.habits) drawHabitsStrip(renderer);
}

}  // namespace inkhome::sleep
